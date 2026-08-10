#include <assert.h>
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
#include <dirent.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "file.h"
#include "msg.h"
#include "user.h"
#include "util.h"
#include "session.h"
#include "server.h"

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
void handle_mkdir(int sockfd, Message* msg);

static Session* sessions;
static size_t nsess;

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

static void set_blocking(int fd, bool is_blocking)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl F_GETFL");
        exit(1);
    }

    int err = fcntl(fd, F_SETFL, is_blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
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
    set_blocking(server_fd, false);

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

static int epfd = -1;
int main(void)
{
    // TODO: use sigaction instead of signal
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
    nsess += NTHREADS;

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

    epfd = epoll_create1(0);
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
    printf("[main] added server fd to epoll: %d\n", server_fd);

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
            printf("[main] fd %d is ready\n", fd);

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
                set_blocking(client_fd, false);

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
                    if (!(errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        printf("[main] error when trying to receive message: %s\n",
                            strerror(errno));
                        on_client_disconnected(fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                        perror("msg_recv_header");
                    }
                    continue;
                }

                if (ret == -2)
                {
                    // TODO: log intentional, clean disconnection
                    printf("[main] client closed connection\n");
                    on_client_disconnected(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                ret = on_client_message_received(fd, &msg);
                if (ret == -1)
                {
                    printf("[main] client disconnected after sending message\n");
                    on_client_disconnected(fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
            }
            else if (events[i].events & (EPOLLERR | EPOLLHUP))
            {
                printf("[main] epoll error or hangup caused by client\n");
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
        printf("[main] non authenticated user sent %s message\n", msg_type_to_str(msg->hdr.type));
        msg_init(msg, MSGTYPE_AUTH_FAIL, NULL, 0);
        msg_send(msg, sockfd, NULL, 0);
        return -1;
    }

    switch (msg->hdr.type)
    {
    case MSGTYPE_AUTH_REQ:
    case MSGTYPE_REMOVE_REQ:
    case MSGTYPE_MKDIR_REQ:
        on_oneshot_req(sockfd, msg);
        break;
    // stream requests
    case MSGTYPE_UPLOAD_REQ:
    case MSGTYPE_DOWNLOAD_REQ:
    case MSGTYPE_LIST_REQ:
        epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
        set_blocking(sockfd, true);
        on_stream_req(sockfd, msg);
        break;
    case MSGTYPE_AUTH_OK:
    case MSGTYPE_AUTH_FAIL:
    case MSGTYPE_UPLOAD_RES:
    case MSGTYPE_UPLOAD_FIN:
    case MSGTYPE_DOWNLOAD_RES:
    case MSGTYPE_DOWNLOAD_FIN:
    case MSGTYPE_LIST_RES:
    case MSGTYPE_REMOVE_RES:
    case MSGTYPE_MKDIR_RES:
    case MSGTYPE_SEND_CHUNK:
    case MSGTYPE_FILEINFO:
    case MSGTYPE_CHUNK_OK:
    case MSGTYPE_CHUNK_AGAIN:
    case MSGTYPE_ERROR:
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
    case MSGTYPE_MKDIR_REQ:
        handle_mkdir(sockfd, msg);
        break;
    case MSGTYPE_NONE:
    case MSGTYPE_ERROR:
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
    case MSGTYPE_REMOVE_RES:
    case MSGTYPE_MKDIR_RES:
    case MSGTYPE_SEND_CHUNK:
    case MSGTYPE_FILEINFO:
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
    case MSGTYPE_ERROR:
    case MSGTYPE_AUTH_REQ:
    case MSGTYPE_AUTH_OK:
    case MSGTYPE_AUTH_FAIL:
    case MSGTYPE_UPLOAD_RES:
    case MSGTYPE_UPLOAD_FIN:
    case MSGTYPE_DOWNLOAD_RES:
    case MSGTYPE_DOWNLOAD_FIN:
    case MSGTYPE_LIST_RES:
    case MSGTYPE_REMOVE_REQ:
    case MSGTYPE_REMOVE_RES:
    case MSGTYPE_MKDIR_RES:
    case MSGTYPE_MKDIR_REQ:
    case MSGTYPE_FILEINFO:
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
            fprintf(stderr, "[main] user dir '%s' already exists\n", user_dir);
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

Session* get_session(uuid_t token, FileInfo* info)
{
    Session* session = NULL;
    for (size_t i = 0; i < nsess; i++)
    {
        if (sessions[i].user && uuid_compare(sessions[i].user->token, token) == 0)
        {
            session = &sessions[i];
            break;
        }
    }

    if (session == NULL)
    {
        for (size_t i = 0; i < nsess; i++)
        {
            if (sessions[i].user == NULL)
            {
                User* user = malloc(sizeof(*user));
                user->total_space = MAX_USER_SPACE;
                user->used_space = MAX_USER_SPACE / 2;
                memcpy(user->token, token, sizeof(user->token));

                session_init(&sessions[i], user, info, SESS_UPLOAD);
                session = &sessions[i];
                break;
            }
        }
    }

    return session;
}

void handle_upload(int sockfd, Message* msg)
{
    fprintf(stderr, "[handle_upload] received upload req\n");
    msg_recv_payload(sockfd, msg, sizeof(FileInfo));

    FileInfo* info = (FileInfo*)msg->payload;
    fileinfo_print(info);

    Session* session = get_session(msg->hdr.token, info);
    if (session == NULL)
    {
        printf("Error: maximum sessions reached");
        send_error(sockfd, "maximum sessions reached");
        return;
    }

    if (session->user->used_space + info->size > session->user->total_space)
    {
        send_error(sockfd, "user storage space is not sufficient");
        return;
    }

    char root_dir[PATH_MAX];
    char token_str[37];
    uuid_unparse(msg->hdr.token, token_str);
    snprintf(root_dir, sizeof(root_dir), STORAGE_DIR "/%s", token_str);

    int ret = mkdir(root_dir, 0700);
    if (ret == -1 && errno != EEXIST)
    {
        send_error(sockfd, "failed to create user directory");
        return;
    }

    char* tmp = strdup(info->filename);
    char* user_path = dirname(tmp);
    ret = create_directories_from_path(root_dir, user_path);
    if (ret == -1)
    {
        send_error(sockfd, "invalid path");
        return;
    }

    send_upload_res(sockfd, NULL, 0);

    char filename[PATH_MAX * 2];
    snprintf(filename, sizeof(filename), "%s/%s", root_dir, info->filename);
    strncpy(info->filename, filename, sizeof(info->filename));

    printf("recving file %s\n", filename);
    ssize_t res = file_recv(sockfd, info);
    printf("file recvd %zd\n", res);
    if (res == -1)
    {
        printf("failed to recv file\n");
        send_error(sockfd, "could not recv file");
        return;
    }

    Message succ_msg = { 0 };
    char* upload_succ = "file successfully uploaded";
    msg_init(&succ_msg, MSGTYPE_UPLOAD_FIN, (void*)upload_succ, strlen(upload_succ) + 1);
    msg_send(&succ_msg, sockfd, NULL, 0);

    free(tmp);
}

void handle_download(int sockfd, Message* msg)
{
    fprintf(stderr, "[handle_download] received download req\n");

    msg_recv_payload(sockfd, msg, sizeof(FileInfo));
    FileInfo* info = (FileInfo*)msg->payload;
    fileinfo_print(info);

    char root_dir[PATH_MAX];
    char token_str[37];
    uuid_unparse(msg->hdr.token, token_str);
    snprintf(root_dir, sizeof(root_dir), STORAGE_DIR "/%s", token_str);

    char filename[PATH_MAX * 2];
    snprintf(filename, sizeof(filename), "%s/%s", root_dir, info->filename);
    strncpy(info->filename, filename, sizeof(info->filename));

    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        if (errno != ENOENT)
        {
            send_error(sockfd, "error while checking if file exists");
            perror("open");
            return;
        }

        send_error(sockfd, "file doesn't exist");
        return;
    }

    printf("file exists!\n");
    fileinfo_from_filename(filename, info);
    printf("Sending full info back to client");
    fileinfo_print(info);

    Session* session = get_session(msg->hdr.token, info);
    if (session == NULL)
    {
        printf("Error: maximum sessions reached");
        send_error(sockfd, "maximum sessions reached");
        return;
    }

    send_download_res(sockfd, (void*)info, sizeof(*info));

    printf("sending file %s\n", filename);
    ssize_t res = file_send(sockfd, info->filename);
    printf("sent file: res=%zd\n", res);
}

void handle_list(int sockfd, Message* msg)
{
    fprintf(stderr, "[handle_list] received list req\n");
    int ret = msg_recv_payload(sockfd, msg, PATH_MAX);
    if (ret == -1)
    {
        printf("error when receiving LIST_REQ message: ret = %d, errno = %d (%s)\n", ret, errno,
            strerror(errno));
        return;
    }
    if (ret == -2)
    {
        printf("peer disconnected gracefully while receiving LIST_REQ: ret = %d\n", ret);
        return;
    }
    if (ret == -3)
    {
        printf("received payload bigger than max\n");
        return;
    }

    char* path = (char*)msg->payload;
    char root_dir[PATH_MAX];
    char token[37];
    uuid_unparse(msg->hdr.token, token);
    snprintf(root_dir, sizeof(root_dir), "%s/%s", STORAGE_DIR, token);
    char abs_path[PATH_MAX * 2];
    snprintf(abs_path, sizeof(abs_path), "%s/%s", root_dir, path);
    char* normalized = normalize_path(abs_path);
    if (normalized == NULL)
    {
        DEBUG_PRINTF("Failed to normalize path '%s'\n", path);
        return;
    }

    if (strncmp(normalized, root_dir, strlen(root_dir)) != 0)
    {
        // user tried a path traversal attack
        printf("client sent invalid path %s\n", path);
        send_error(sockfd, "invalid path");
        return;
    }

    DEBUG_PRINTF("normalized: %s\n", normalized);

    DIR* dir = opendir(normalized);
    if (dir == NULL)
    {
        perror("opendir");
        send_error(sockfd, strerror(errno));
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", normalized, entry->d_name);

        msg_init(msg, MSGTYPE_FILEINFO, NULL, sizeof(FileInfo));
        msg->payload = malloc(sizeof(FileInfo));
        assert(msg->payload != NULL && "buy more RAM!");
        memset(msg->payload, 0, sizeof(FileInfo));

        if (entry->d_type == DT_REG)
        {
            fileinfo_from_filename(filepath, (void*)msg->payload);
        }

        strcpy(((FileInfo*)msg->payload)->filename, entry->d_name);

        msg_send(msg, sockfd, NULL, 0);
        free(msg->payload);
    }

    msg_init(msg, MSGTYPE_NONE, NULL, 0);
    ret = msg_send(msg, sockfd, NULL, 0);
    if (ret < 0)
    {
        printf("failed to send MSGTYPE_NONE to terminate listing: %s\n", strerror(errno));
    }
    printf("sent MSGTYPE_NONE to terminate listing\n");
}

void handle_remove(int sockfd, Message* msg)
{
    fprintf(stderr, "[handle_remove] received remove req\n");
    int ret = msg_recv_payload(sockfd, msg, 8192);
    if (ret < 0)
    {
        switch (ret)
        {
        case -1:
            printf("error when receiving MSGTYPE_REMOVE_REQ message: ret = %d, errno = %d (%s)\n",
                ret, errno, strerror(errno));
            break;
        case -2:
            printf("peer disconnected gracefully while receiving MSGTYPE_REMOVE_REQ: ret = %d\n",
                ret);
            break;
        case -3:
            printf("received payload bigger than max\n");
            break;
        }

        return;
    }

    char* filepath = get_user_path((char*)msg->payload, msg->hdr.token);
    if (filepath == NULL)
    {
        send_error(sockfd, "invalid path");
        return;
    }

    DEBUG_PRINTF("normalized: %s\n", filepath);

    FileInfo info = { 0 };
    ret = fileinfo_from_filename(filepath, &info);
    if (ret < 0 && ret != -2)
    {
        switch (ret)
        {
        case -1:
            printf("failed to stat file\n");
            break;
        case -3:
            printf("failed to calculate file checksum\n");
            break;
        }
        send_error(sockfd, "failed to get fileinfo from filename\n");
        return;
    }

    if (ret == -2)
    {
        strcpy(info.filename, filepath);
    }

    if (rm_r(filepath) == -1)
    {
        send_error(sockfd, strerror(errno));
        perror("unlink");
        return;
    }

    DEBUG_PRINTF("file %s successfully deleted\n", info.filename);

    msg_init(msg, MSGTYPE_REMOVE_RES, (byte*)&info, sizeof(info));
    ret = msg_send(msg, sockfd, NULL, 0);
    if (ret < 0)
    {
        perror("msg_send(MSGTYPE_REMOVE_RES)");
        return;
    }

    DEBUG_PRINTF("sent FileInfo of deleted file\n");
}

void handle_mkdir(int sockfd, Message* msg)
{
    int ret = msg_recv_payload(sockfd, msg, 8192);
    if (!handle_recv_error(ret, MSGTYPE_MKDIR_REQ)) return;

    char root_dir[PATH_MAX];
    get_user_root(msg->hdr.token, root_dir);

    ret = create_directories_from_path(root_dir, (char*)msg->payload);
    if (ret < 0)
    {
        switch (ret)
        {
        case -1:
        case -2:
            send_error(sockfd, "invalid path");
            break;
        case -3:
            send_error(sockfd, strerror(errno));
            break;
        }
        return;
    }

    DEBUG_PRINTF("directory successfully created\n");

    msg_init(msg, MSGTYPE_MKDIR_RES, NULL, 0);
    ret = msg_send(msg, sockfd, NULL, 0);
    if (ret < 0)
    {
        perror("msg_send(MSGTYPE_MKDIR_RES)");
        return;
    }

    DEBUG_PRINTF("sent MSGTYPE_MKDIR_RES\n");
}
