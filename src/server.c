#include <signal.h>
#include <stddef.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>

#include "msg.h"

#define PORT 1234
#define NTHREADS 10UL

#include "session.h"

typedef struct {
    int id;
    pthread_t thread;
    sem_t work_sem;
    Session session;
    bool terminated;
    int sockfd;
    void (*func)(int sockfd);
} Worker;

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
        w->func(w->sockfd);
    }

    free(w);

    return NULL;
}

void task_func(int sockfd)
{
    Message msg;
    msg_recv(sockfd, &msg);

    char token[37];
    uuid_unparse(msg.data, token);

    printf("Received message: \n");
    printf("    type: %s\n", msg_type_to_str(msg.hdr.type));
    printf("    length: %zu\n", msg.hdr.length);
    printf("    sent_at: %zus, %zuns\n", msg.hdr.sent_at.tv_sec, msg.hdr.sent_at.tv_nsec);
    printf("    data: %s\n", token);

    while (true)
        ;
}

Worker* workers[NTHREADS];

void cleanup(void)
{
    fprintf(stderr, "[main] destroying workers...\n");
    for (size_t i = 0; i < NTHREADS; i++)
    {
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

int main(void)
{
    signal(SIGINT, sighandler);
    atexit(cleanup);

    fprintf(stderr, "[main] creating %lu workers...\n", NTHREADS);
    for (size_t i = 0; i < NTHREADS; i++)
    {
        workers[i] = malloc(sizeof(Worker));
        workers[i]->terminated = false;
        workers[i]->id = i;
        sem_init(&workers[i]->work_sem, 0, 0);
        pthread_create(&workers[i]->thread, 0, worker, workers[i]);
    }
    fprintf(stderr, "[main] workers ready!\n");

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    /*
     * Configure address
     */
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    /*
     * Bind socket to address
     */
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("Listening at 0.0.0.0:%d\n\n", PORT);

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

        workers[0]->sockfd = sockfd;
        workers[0]->func = task_func;
        sem_post(&workers[0]->work_sem);
        printf("New client connected, assigned worker thread %lu\n", 0UL);
    }

    return 0;
}
