#include <assert.h>

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

    msg_init(&msg, SEND_CHUNK, self->data, self->hdr.length);

    msg_send(&msg, sockfd, (byte*)&self->hdr, sizeof(self->hdr));

    return self->hdr.length;
}

ssize_t chunk_recv(int sockfd, Chunk* self);
