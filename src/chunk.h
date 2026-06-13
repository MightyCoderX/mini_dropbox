#ifndef CHUNK_PROTO_H
#define CHUNK_PROTO_H

#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "types.h"
#include "util.h"

#define CHUNK_SIZE 4096

typedef struct {
    size_t seq;
    size_t start_byte;
    size_t end_byte;
    size_t length;
    checksum_t checksum;
} ChunkHdr;

typedef struct {
    ChunkHdr hdr;
    byte* data;
} Chunk;

void chunk_init(Chunk* self, ChunkHdr hdr, byte* data, size_t length);
ssize_t chunk_send(Chunk* self, int sockfd);
ssize_t chunk_recv(int sockfd, Chunk* self);

#endif // !CHUNK_PROTO_H
