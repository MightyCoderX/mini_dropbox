#ifndef _SERVER_H
#define _SERVER_H

#include <semaphore.h>

#include "session.h"

#define PORT 1234
#define NTHREADS 10UL
#define STORAGE_DIR "/var/minibox"
#define USER_MAX_STORAGE 10e9
#define MAX_EVENTS 64

typedef struct Worker Worker;

struct Worker {
    int id;
    pthread_t thread;
    sem_t work_sem;
    Session* session;
    // TODO: use atomic variable instead of boolean updated in sigaction handler
    bool terminated;
    int sockfd;
    void (*func)(Worker* w);
};
#endif // !_SERVER_H
