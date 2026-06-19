#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "msg.h"
#include "util.h"

// Chunk* chunk_next(FileInfo* info)
// {
//     static FileInfo* s_info;
//     static size_t byte_index;
//     static int fd;
//
//     if (info != NULL)
//     {
//         s_info = info;
//         byte_index = 0;
//         fd = 0;
//         return NULL;
//     }
//
//     // TODO: create chunk, increase byte index by CHUNK_SIZE, return chunk
//
//     return NULL;
// }

void chunk_init(Chunk* self, ChunkHdr hdr, byte* data, size_t length)
{
    self->hdr = hdr;
    checksum(data, length, hdr.checksum);
}

ssize_t chunk_send(Chunk* self, int sockfd)
{
    Message msg;

    msg_init(&msg, MSGTYPE_SEND_CHUNK, self->data, self->hdr.length);

    msg_send(&msg, sockfd, (byte*)&self->hdr, sizeof(self->hdr));

    return self->hdr.length;
}

ssize_t chunk_recv(int sockfd, Chunk* self);
