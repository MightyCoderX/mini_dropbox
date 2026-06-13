#include <arpa/inet.h>
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

#include "msg.h"

#define PORT 1234
#define NTHREADS 10UL
#define STORAGE_DIR "/var/minibox"
#define USER_MAX_STORAGE 10e9

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

void handle_auth(Worker* w, Message* msg)
{
    char token[37];
    uuid_unparse(msg->hdr.token, token);

    fprintf(stderr, "[worker #%d] user with token %s requested auth\n", w->id, token);

    char user_dir[PATH_MAX];
    snprintf(user_dir, sizeof(user_dir), STORAGE_DIR "/%s", token);
    if (mkdir(user_dir, 0700) < 0)
    {
        Message res;
        char error[100];

        strcpy(error, strerror(errno));
        fprintf(stderr, "[worker #%d] failed to create user dir: %s\n", w->id, error);
        if (errno == EEXIST)
        {
            strcpy(error, "Already registered");
            fprintf(stderr, "[worker #%d] user dir already exists\n", w->id);
        }
        msg_init(&res, MSGTYPE_AUTH_FAIL, (byte*)error, strlen(error));
        msg_send(&res, w->sockfd, NULL, 0);
    }
    else
    {
        fprintf(stderr, "[worker #%d] new user dir created\n", w->id);
        Message res;
        char succ[] = "Registered\0";
        msg_init(&res, MSGTYPE_AUTH_OK, (byte*)succ, sizeof(succ));
        msg_send(&res, w->sockfd, NULL, 0);
    }
}

void handle_upload(Worker* w, Message* msg)
{
    (void)w;
    fprintf(stderr, "[handle_upload] received upload req\n");

    printf("receiving payload\n");
    msg_recv_payload(w->sockfd, msg, sizeof(FileInfo));
    printf("received %zu bytes\n", msg->hdr.length);

    FileInfo* info = (FileInfo*)msg->payload;

    printf("filename: %s\n", info->name);
    printf("size: %zu\n", info->size);
    printf("chunk count: %zu\n", info->chunk_count);
}

void handle_download(Worker* w, Message* msg)
{
    (void)w;
    (void)msg;
    fprintf(stderr, "[handle_download] received download req\n");
}

void handle_list(Worker* w, Message* msg)
{
    (void)w;
    (void)msg;
    fprintf(stderr, "[handle_list] received list req\n");
}

void handle_remove(Worker* w, Message* msg)
{
    (void)w;
    (void)msg;
    fprintf(stderr, "[handle_remove] received remove req\n");
}

static Session* sessions;

void handle_client(Worker* w)
{
    Message msg;
    msg_recv_header(w->sockfd, &msg);

    char token[37];
    uuid_unparse(msg.hdr.token, token);
    printf("Received message: \n");
    printf("    type: %s\n", msg_type_to_str(msg.hdr.type));
    printf("    length: %zu\n", msg.hdr.length);
    printf("    sent_at: %zus, %zuns\n", msg.hdr.sent_at.tv_sec, msg.hdr.sent_at.tv_nsec);
    printf("    token: %s\n", token);
    printf("    rcvd_at: %zus, %zuns\n", msg.rcvd_at.tv_sec, msg.rcvd_at.tv_nsec);

    if (msg.hdr.type != MSGTYPE_AUTH_REQ && uuid_is_null(msg.hdr.token))
    {
        msg_init(&msg, MSGTYPE_AUTH_FAIL, NULL, 0);
        msg_send(&msg, w->sockfd, NULL, 0);
        return;
    }

    for (size_t i = 0; i < NTHREADS; i++)
    {
        if (sessions[i].user->token == msg.hdr.token)
        {
            w->session = &sessions[i];
            break;
        }
    }

    switch (msg.hdr.type)
    {
    case MSGTYPE_AUTH_REQ:
        handle_auth(w, &msg);
        break;
    case MSGTYPE_UPLOAD_REQ:
        handle_upload(w, &msg);
        break;
    case MSGTYPE_DOWNLOAD_REQ:
        handle_download(w, &msg);
        break;
    case MSGTYPE_LIST_REQ:
        break;
    case MSGTYPE_REMOVE_REQ:
        handle_remove(w, &msg);
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
        break;
    case MSGTYPE_NONE:
        break;
    }
}

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
    socklen_t addrlen = sizeof(address);
    int server_fd = create_server_socket("0.0.0.0", PORT, &address);

    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("[main] Listening at 0.0.0.0:%d\n\n", PORT);

    /*
     * Accept new connections
     */
    while (true)
    {
        int sockfd = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (sockfd < 0)
        {
            perror("accept");
            continue;
        }

        // TODO: actually assign a free worker here instead of the same hard-coded one
        workers[0]->sockfd = sockfd;
        workers[0]->func = handle_client;
        sem_post(&workers[0]->work_sem);
        printf("New client connected, assigned worker thread %lu\n", 0UL);
    }

    return 0;
}
