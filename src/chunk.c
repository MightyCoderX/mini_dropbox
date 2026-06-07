#include <assert.h>
#include <stdlib.h>

#include "chunk.h"
#include "msg.h"

Chunk chunk(ChunkHdr hdr, byte* data)
{
    return (Chunk) {
        hdr,
        data,
    };
}

ssize_t chunk_send(Chunk* self, int sockfd)
{
    Message msg;

    size_t bufsize = sizeof(ChunkHdr) + CHUNK_SIZE;
    byte* buf = malloc(bufsize);

    msg_init(&msg, SEND_CHUNK, buf, bufsize);

    msg_send(&msg, sockfd);

    return self->hdr.length;
}

ssize_t chunk_recv(int sockfd, Chunk* self);
