#ifndef CHUNK_PROTO_H
#define CHUNK_PROTO_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#define CHUNK_SIZE 4096

typedef struct {
    size_t seq;
    size_t start;
    size_t end;
    size_t length;
    unsigned char* data;
} Chunk;

void chunk_init(size_t start, size_t end);
ssize_t chunk_send(Chunk* self, int sockfd);
ssize_t chunk_recv(Chunk* self, int sockfd);

#endif // !CHUNK_PROTO_H
