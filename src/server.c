#include <arpa/inet.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <linux/limits.h>
#include <uuid/uuid.h>
#include <sys/epoll.h>

#include "msg.h"

#define PORT 1234
#define NTHREADS 10UL
#define STORAGE_DIR "/var/minibox"
#define USER_MAX_STORAGE 10e9
#define MAX_EVENTS 64

#include "session.h"

typedef struct Worker Worker;
struct Worker {
    int id;
    pthread_t thread;
    sem_t work_sem;
    Session* session;
    bool terminated;
    int sockfd;
    void (*func)(Worker* w);
};

void* worker(void* arg)
{
    Worker* w = arg;

    while (true)
    {
        sem_wait(&w->work_sem);
        if (w->terminated)
        {
            fprintf(stderr, "[worker #%d] terminated, exiting...\n", w->id);
            break;
        }

        if (w->func == NULL)
        {
            fprintf(stderr, "[worker #%d] missing task function, going to sleep!\n", w->id);
            continue;
        }
        w->func(w);
    }

    free(w);

    return NULL;
}

void on_client_connected(int client_fd, struct sockaddr_in client_addr);
int on_client_message_received(int client_fd, Message* msg);
void on_client_disconnected(int client_fd);

void handle_client(Worker* w);

void on_oneshot_req(int sockfd, Message* msg);
void on_stream_req(int sockfd, Message* msg);

void handle_auth(int sockfd, Message* msg);
void handle_upload(int sockfd, Message* msg);
void handle_download(int sockfd, Message* msg);
void handle_list(int sockfd, Message* msg);
void handle_remove(int sockfd, Message* msg);

static Session* sessions;

Worker* workers[NTHREADS];

void cleanup(void)
{
    fprintf(stderr, "[main] destroying workers...\n");
    for (size_t i = 0; i < NTHREADS; i++)
    {
        if (workers[i] == NULL) continue;

        workers[i]->terminated = true;
        sem_post(&workers[i]->work_sem);
        fprintf(stderr, "[main] signaled worker %zu\n", i);
        int ret = pthread_join(workers[i]->thread, NULL);
        if (ret == -1)
        {
            perror("pthread_join");
            continue;
        }
        sem_destroy(&workers[i]->work_sem);
    }
    fprintf(stderr, "[main] workers destroyed successfully!\n");
}

void sighandler(int sig)
{
    switch (sig)
    {
    case SIGINT:
        cleanup();
        _exit(0);
        break;
    }
}

static void set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        exit(1);
    }

    int err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (err == -1)
    {
        perror("fcntl F_SETFL");
        exit(1);
    }
}

int create_server_socket(const char* server_ip, short port, struct sockaddr_in* address)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return -1;
    }

    // Set option to avoid bind errors
    // when TCP socket doesn't close properly
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt SO_REUSEADDR");
        return -1;
    }
    set_nonblock(server_fd);

    /*
     * Configure address
     */
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    int ret = inet_pton(AF_INET, server_ip, &address->sin_addr);
    if (ret == 0)
    {
        return -2;
    }
    else if (ret == -1)
    {
        perror("inet_pton AF_INET");
        return -1;
    }

    /*
     * Bind socket to address
     */
    if (bind(server_fd, (struct sockaddr*)address, sizeof(*address)) < 0)
    {
        perror("bind");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int main(void)
{
    signal(SIGINT, sighandler);
    atexit(cleanup);

    if (mkdir(STORAGE_DIR, 0700) < 0)
    {
        if (errno == EEXIST)
        {
            fprintf(stderr, "[main] storage directory already exists\n");
        }
        else
        {
            perror(STORAGE_DIR ": mkdir");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "[main] created storage directory %s\n", STORAGE_DIR);
    }

    sessions = calloc(NTHREADS, sizeof(*sessions));
    if (sessions == NULL)
    {
        fprintf(stderr, "[main] error while allocating sessions: %s\n", strerror(errno));
        return 1;
    }

    fprintf(stderr, "[main] creating %lu workers...\n", NTHREADS);
    for (size_t i = 0; i < NTHREADS; i++)
    {
        workers[i] = malloc(sizeof(Worker));
        workers[i]->terminated = false;
        workers[i]->id = i;
        workers[i]->session = NULL;
        sem_init(&workers[i]->work_sem, 0, 0);
        pthread_create(&workers[i]->thread, 0, worker, workers[i]);
    }
    fprintf(stderr, "[main] workers ready!\n");

    struct sockaddr_in address;
    int server_fd = create_server_socket("0.0.0.0", PORT, &address);

    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("[main] Listening at 0.0.0.0:%d\n\n", PORT);

    int epfd = epoll_create1(0);
    if (epfd == -1)
    {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = server_fd,
    };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1)
    {
        perror("epoll_ctl ADD server_fd");
        return 1;
    }

    /*
     * Accept new connections
     */
    struct epoll_event events[MAX_EVENTS];
    while (1)
    {
        int nevents = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (nevents == -1)
        {
            // continue if interrupted by signal
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < nevents; i++)
        {
            int fd = events[i].data.fd;
            if (fd == server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t addrlen = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
                if (client_fd < 0)
                {
                    perror("accept");
                    continue;
                }
                set_nonblock(client_fd);

                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev) == -1)
                {
                    perror("epoll_ctl ADD client");
                    close(client_fd);
                }

                // TODO: log connection
                on_client_connected(client_fd, client_addr);
            }
            else if (events[i].events & EPOLLIN) // event on a client fd: data or disconnection
            {
                Message msg;
                int ret = msg_recv_header(fd, &msg);
                if (ret == -1)
                {
                    // TODO: log system error, unintended disconnection
                    on_client_disconnected(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    perror("msg_recv_header");
                    continue;
                }

                if (ret == -2)
                {
                    // TODO: log intentional, clean disconnection
                    on_client_disconnected(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }

                ret = on_client_message_received(fd, &msg);
                if (ret == -1)
                {
                    on_client_disconnected(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
            else if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
                // TODO: client disconnected cause of epoll error/hangup
                on_client_disconnected(fd);
            }
        }

        // workers[0]->sockfd = sockfd;
        // workers[0]->func = handle_client;
        // sem_post(&workers[0]->work_sem);
        // printf("New client connected, assigned worker thread %lu\n", 0UL);
    }

    // FIX: never reached, put this in cleanup
    close(epfd);
    close(server_fd);
    return 0;
}

void on_client_connected(int client_fd, struct sockaddr_in client_addr)
{
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));

    printf("[+] new client\n");
    printf("    connection socket: fd=%d, %s:%d\n", client_fd, ip, ntohs(client_addr.sin_port));
}

int on_client_message_received(int sockfd, Message* msg)
{
    char token[37];
    uuid_unparse(msg->hdr.token, token);
    printf("Received message: \n");
    printf("    type: %s\n", msg_type_to_str(msg->hdr.type));
    printf("    length: %zu\n", msg->hdr.length);
    printf("    sent_at: %zus, %zuns\n", msg->hdr.sent_at.tv_sec, msg->hdr.sent_at.tv_nsec);
    printf("    token: %s\n", token);
    printf("    rcvd_at: %zus, %zuns\n", msg->rcvd_at.tv_sec, msg->rcvd_at.tv_nsec);

    if (msg->hdr.type != MSGTYPE_AUTH_REQ && uuid_is_null(msg->hdr.token))
    {
        msg_init(msg, MSGTYPE_AUTH_FAIL, NULL, 0);
        msg_send(msg, sockfd, NULL, 0);
        return -1;
    }

    switch (msg->hdr.type)
    {
    case MSGTYPE_AUTH_REQ:
    case MSGTYPE_REMOVE_REQ:
        on_oneshot_req(sockfd, msg);
        break;
    // stream requests
    case MSGTYPE_UPLOAD_REQ:
    case MSGTYPE_DOWNLOAD_REQ:
    case MSGTYPE_LIST_REQ:
        on_stream_req(sockfd, msg);
        break;
    case MSGTYPE_AUTH_OK:
    case MSGTYPE_AUTH_FAIL:
    case MSGTYPE_UPLOAD_RES:
    case MSGTYPE_UPLOAD_FIN:
    case MSGTYPE_DOWNLOAD_RES:
    case MSGTYPE_DOWNLOAD_FIN:
    case MSGTYPE_LIST_RES:
    case MSGTYPE_REMOVE_OK:
    case MSGTYPE_REMOVE_FAIL:
    case MSGTYPE_SEND_CHUNK:
    case MSGTYPE_CHUNK_OK:
    case MSGTYPE_CHUNK_AGAIN:
        fprintf(stderr, "invalid message received %s\n", msg_type_to_str(msg->hdr.type));
        break;
    case MSGTYPE_NONE:
        break;
    }
    return 0;
}

void on_client_disconnected(int client_fd)
{
    (void)client_fd;
}

void on_oneshot_req(int sockfd, Message* msg)
{
    switch (msg->hdr.type)
    {
    case MSGTYPE_AUTH_REQ:
        handle_auth(sockfd, msg);
        break;
    case MSGTYPE_REMOVE_REQ:
        handle_remove(sockfd, msg);
        break;
    case MSGTYPE_NONE:
    case MSGTYPE_AUTH_OK:
    case MSGTYPE_AUTH_FAIL:
    case MSGTYPE_UPLOAD_REQ:
    case MSGTYPE_UPLOAD_RES:
    case MSGTYPE_UPLOAD_FIN:
    case MSGTYPE_DOWNLOAD_REQ:
    case MSGTYPE_DOWNLOAD_RES:
    case MSGTYPE_DOWNLOAD_FIN:
    case MSGTYPE_LIST_REQ:
    case MSGTYPE_LIST_RES:
    case MSGTYPE_REMOVE_OK:
    case MSGTYPE_REMOVE_FAIL:
    case MSGTYPE_SEND_CHUNK:
    case MSGTYPE_CHUNK_OK:
    case MSGTYPE_CHUNK_AGAIN:
        break;
    }
}

void on_stream_req(int sockfd, Message* msg)
{
    switch (msg->hdr.type)
    {
    case MSGTYPE_UPLOAD_REQ:
        handle_upload(sockfd, msg);
        break;
    case MSGTYPE_DOWNLOAD_REQ:
        handle_download(sockfd, msg);
        break;
    case MSGTYPE_LIST_REQ:
        handle_list(sockfd, msg);
        break;
    case MSGTYPE_NONE:
    case MSGTYPE_AUTH_REQ:
    case MSGTYPE_AUTH_OK:
    case MSGTYPE_AUTH_FAIL:
    case MSGTYPE_UPLOAD_RES:
    case MSGTYPE_UPLOAD_FIN:
    case MSGTYPE_DOWNLOAD_RES:
    case MSGTYPE_DOWNLOAD_FIN:
    case MSGTYPE_LIST_RES:
    case MSGTYPE_REMOVE_REQ:
    case MSGTYPE_REMOVE_OK:
    case MSGTYPE_REMOVE_FAIL:
    case MSGTYPE_SEND_CHUNK:
    case MSGTYPE_CHUNK_OK:
    case MSGTYPE_CHUNK_AGAIN:
        break;
    }
}

void handle_auth(int sockfd, Message* msg)
{
    char token[37];
    uuid_unparse(msg->hdr.token, token);

    fprintf(stderr, "[main] user with token %s requested auth\n", token);

    char user_dir[PATH_MAX];
    snprintf(user_dir, sizeof(user_dir), STORAGE_DIR "/%s", token);
    if (mkdir(user_dir, 0700) < 0)
    {
        Message res;
        char error[100];

        strcpy(error, strerror(errno));
        fprintf(stderr, "[main] failed to create user dir: %s\n", error);
        if (errno == EEXIST)
        {
            strcpy(error, "Already registered");
            fprintf(stderr, "[main] user dir already exists\n");
        }
        msg_init(&res, MSGTYPE_AUTH_FAIL, (byte*)error, strlen(error));
        msg_send(&res, sockfd, NULL, 0);
    }
    else
    {
        fprintf(stderr, "[main] new user dir created\n");
        Message res;
        char succ[] = "Registered\0";
        msg_init(&res, MSGTYPE_AUTH_OK, (byte*)succ, sizeof(succ));
        msg_send(&res, sockfd, NULL, 0);
    }
}

void handle_upload(int sockfd, Message* msg)
{
    fprintf(stderr, "[handle_upload] received upload req\n");

    printf("receiving payload\n");
    msg_recv_payload(sockfd, msg, sizeof(FileInfo));
    printf("received %zu bytes\n", msg->hdr.length);

    FileInfo* info = (FileInfo*)msg->payload;

    printf("filename: %s\n", info->name);
    printf("size: %zu\n", info->size);
    printf("chunk count: %zu\n", info->chunk_count);
}

void handle_download(int sockfd, Message* msg)
{
    (void)sockfd;
    (void)msg;
    fprintf(stderr, "[handle_download] received download req\n");
}

void handle_list(int sockfd, Message* msg)
{
    (void)sockfd;
    (void)msg;
    fprintf(stderr, "[handle_list] received list req\n");
}

void handle_remove(int sockfd, Message* msg)
{
    (void)sockfd;
    (void)msg;
    fprintf(stderr, "[handle_remove] received remove req\n");
}
