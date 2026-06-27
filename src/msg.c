#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bits/time.h>
#include <sys/socket.h>
#include <uuid/uuid.h>

#include "msg.h"
#include "types.h"

void msg_init(Message* self, MessageType type, byte* payload, size_t length)
{
    self->hdr.length = length;
    self->hdr.type = type;
    self->hdr.sent_at = (struct timespec) { 0, 0 };
    self->rcvd_at = (struct timespec) { 0, 0 };
    self->payload = payload;
    self->payload_len = length;
}

void msg_clear(Message* self)
{
    self->hdr = (MessageHdr) { 0 };
    self->rcvd_at = (struct timespec) { 0 };
    self->payload = NULL;
    self->payload_len = 0;
}

const char* msg_type_to_str(MessageType type)
{
    switch (type)
    {
#define X(id, name)  \
    case id:         \
        return name;

        MSGTYPES
#undef X
    default:
        assert(0 && "Unreachable");
    }
}

void msg_print(Message* self)
{
    char token[37];
    uuid_unparse(self->hdr.token, token);
    printf("Received message: \n");
    printf("    type: %s\n", msg_type_to_str(self->hdr.type));
    printf("    length: %zu\n", self->hdr.length);
    printf("    sent_at: %zus, %zuns\n", self->hdr.sent_at.tv_sec, self->hdr.sent_at.tv_nsec);
    printf("    token: %s\n", token);
    printf("    rcvd_at: %zus, %zuns\n", self->rcvd_at.tv_sec, self->rcvd_at.tv_nsec);
}

static int send_all(int sockfd, const void* buf, size_t len, int flags)
{
    assert(buf != NULL && len > 0);

    // can't use void* for pointer arithmetic, so we make an alias
    const byte* buf_ptr = buf;
    size_t bytes_left = len;

    while (bytes_left > 0)
    {
        ssize_t nbytes = send(sockfd, buf_ptr, bytes_left, flags | MSG_NOSIGNAL);

        if (nbytes < 0)
        {
            // retry if interrupted by signal
            if (errno == EINTR) continue;

            // return immediately if send failed, leaving errno untouched
            return -1;
        }

        // highly unusual for blocking socket, but if it ever
        // happens it means the kernel can't accept data
        // so we treat it as a disconnection,
        // can only happen if len is 0 but the assert prevents that
        if (nbytes == 0) return -2;

        bytes_left -= nbytes;
        buf_ptr += nbytes;
    }

    return 0;
}

static int recv_all(int sockfd, void* buf, size_t len)
{
    assert(buf != NULL && len > 0);
    byte* buf_ptr = buf;

    ssize_t bytes_left = len;

    // loop until 'n' bytes are received
    while (bytes_left > 0)
    {
        // try to recv all of the bytes
        // MSG_WAITALL tries to read everything but
        // it may still be interrupted by signals
        ssize_t nbytes = recv(sockfd, buf_ptr, bytes_left, MSG_WAITALL);

        // connection was terminated abruptly by an actual error
        if (nbytes < 0)
        {
            if (errno == EINTR) continue;

            return -1;
        }

        // peer disconnected gracefully, no errno
        if (nbytes == 0)
        {
            return -2;
        }

        bytes_left -= nbytes;
        buf_ptr += nbytes;
    }

    return 0;
}

int msg_send(Message* self, int sockfd, byte* payload_hdr, size_t payload_hdr_len)
{
    assert(self != NULL);
    clock_gettime(CLOCK_REALTIME, &self->hdr.sent_at);

    if (payload_hdr) self->hdr.length += payload_hdr_len;

    // we use MSG_MORE to tell the kernel that there is more coming
    // so it can fill the segment with before sending it
    int ret = send_all(sockfd, &self->hdr, sizeof(MessageHdr), MSG_MORE);
    if (ret < 0) return ret;

    if (payload_hdr)
    {
        ret = send_all(sockfd, payload_hdr, payload_hdr_len, MSG_MORE);
        if (ret < 0) return ret;
    }

    if (self->payload)
    {
        ret = send_all(sockfd, self->payload, self->payload_len, 0);
        if (ret < 0) return ret;
    }

    return 0;
}

int msg_recv_header(int sockfd, Message* msg)
{
    assert(msg != NULL);

    int ret = recv_all(sockfd, (void*)&msg->hdr, sizeof(MessageHdr));
    if (ret < 0) return ret;

    clock_gettime(CLOCK_REALTIME, &msg->rcvd_at);

    return 0;
}

int msg_recv_payload(int sockfd, Message* msg, size_t maxlen)
{
    assert(msg != NULL);

    if (msg->hdr.length > maxlen) return -3;

    if (msg->hdr.length > 0)
    {
        msg->payload = malloc(msg->hdr.length);

        int ret = recv_all(sockfd, msg->payload, msg->hdr.length);
        if (ret < 0) return ret;
    }

    return 0;
}

int msg_recv(int sockfd, Message* msg, size_t maxlen)
{
    assert(msg != NULL);

    int ret = recv_all(sockfd, (void*)&msg->hdr, sizeof(MessageHdr));
    if (ret < 0) return ret;

    if (msg->hdr.length > maxlen) return -3;

    if (msg->hdr.length > 0)
    {
        msg->payload = malloc(msg->hdr.length);

        ret = recv_all(sockfd, msg->payload, msg->hdr.length);
        if (ret < 0) return ret;
    }

    clock_gettime(CLOCK_REALTIME, &msg->rcvd_at);

    return 0;
}
