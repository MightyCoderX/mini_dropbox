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
    self->data = data;
    checksum(data, length, hdr.checksum);
}

ssize_t chunk_send(Chunk* self, int sockfd)
{
    Message msg;

    msg_init(&msg, MSGTYPE_SEND_CHUNK, self->data, self->hdr.length);
    int ret = msg_send(&msg, sockfd, (byte*)&self->hdr, sizeof(self->hdr));
    if (ret < 0) return ret;

    return self->hdr.length;
}

ssize_t chunk_recv(int sockfd, Chunk* self)
{
    Message msg = { 0 };
    int ret = msg_recv_header(sockfd, &msg);
    if (ret < 0)
    {
        perror("msg_recv_header");
        return ret;
    }

    if (msg.hdr.length < sizeof(ChunkHdr))
    {
        fprintf(stderr, "chunk received was smaller than ChunkHdr %zu\n", msg.hdr.length);
        return -1;
    }

    msg_recv_payload(sockfd, &msg, sizeof(ChunkHdr) + CHUNK_SIZE);

    memcpy(&self->hdr, msg.payload, sizeof(ChunkHdr));
    self->data = msg.payload + sizeof(ChunkHdr);

    checksum_t chk;
    checksum(self->data, self->hdr.length, chk);

    if (!checksums_match(chk, self->hdr.checksum))
    {
        return -2;
    }

    return 0;
}
