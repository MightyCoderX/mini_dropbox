
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bits/time.h>
#include <sys/socket.h>

#include "msg.h"

void msg_init(Message* self, MessageType type, byte* data, size_t length)
{
    self->hdr.length = length;
    self->hdr.type = type;
    self->hdr.sent_at = (struct timespec) { 0, 0 };
    self->rcvd_at = (struct timespec) { 0, 0 };
    self->data = data;
}

const char* msg_type_to_str(MessageType type)
{
    switch (type)
    {
    case AUTH_REQ:
        return "AUTH_REQ";
    case AUTH_OK:
        return "AUTH_OK";
    case AUTH_FAIL:
        return "AUTH_FAIL";
    case UPLOAD_REQ:
        return "UPLOAD_REQ";
    case UPLOAD_RES:
        return "UPLOAD_RES";
    case UPLOAD_FIN:
        return "UPLOAD_FIN";
    case DOWNLOAD_REQ:
        return "DOWNLOAD_REQ";
    case DOWNLOAD_RES:
        return "DOWNLOAD_RES";
    case DOWNLOAD_FIN:
        return "DOWNLOAD_FIN";
    case SEND_CHUNK:
        return "SEND_CHUNK";
    case CHUNK_OK:
        return "CHUNK_OK";
    case CHUNK_AGAIN:
        return "CHUNK_AGAIN";
    default:
        assert(0 && "Unreachable");
    }
}

static ssize_t send_all(int sockfd, const void* buf, size_t n)
{
    assert(n > 0);

    // can't use void* for pointer arithmetic, so we make an alias
    const byte* buf_pos = buf;
    size_t bytes_sent = 0;
    size_t bytes_left;

    do
    {
        bytes_left = n - bytes_sent;
        ssize_t nbytes = send(sockfd, buf_pos, bytes_left, 0);

        // return immediately if send failed, without changing errno
        // so the caller can handle it
        if (nbytes < 0)
        {
            return nbytes;
        }

        // increment bytes sent
        bytes_sent += nbytes;
        // move pointer to next position
        buf_pos += nbytes;
    } while (bytes_left);

    return bytes_sent;
}

static ssize_t recv_all(int sockfd, void* buf, size_t n)
{
    assert(n > 0);

    // can't use void* for pointer arithmetic, so we make an alias
    byte* buf_pos = buf;
    size_t bytes_rcvd = 0;

    // initializing this here in case 'n' is 0 and
    // the loop body doesn't execute
    ssize_t nbytes = 0;

    // loop until 'n' bytes are received
    while (bytes_rcvd < n)
    {
        // calculate number of bytes left to recv
        size_t bytes_left = n - bytes_rcvd;
        // try to recv all of the bytes left
        nbytes = recv(sockfd, buf_pos, bytes_left, 0);

        // if recv fails, or reads 0 bytes, which means the
        // connection was closed, break out of the loop
        if (nbytes <= 0)
        {
            break;
        }

        bytes_rcvd += nbytes;
        buf_pos += nbytes;
    }

    if (nbytes == 0)
    {
        errno = ECONNRESET;
        return -1;
    }

    // if recv failed return -1, and leave errno untouched
    // to pass it to the caller
    if (nbytes < 0)
    {
        return -1;
    }

    return bytes_rcvd;
}

ssize_t msg_send(Message* self, int sockfd)
{
    assert(self != NULL);
    clock_gettime(CLOCK_REALTIME, &self->hdr.sent_at);

    ssize_t ret = send_all(sockfd, self, sizeof(MsgHdr));
    if (ret < 0)
    {
        return -1;
    }

    ret = send_all(sockfd, self->data, self->hdr.length);
    if (ret < 0)
    {
        return -1;
    }

    return 0;
}

ssize_t msg_recv(int sockfd, Message* msg)
{
    assert(msg != NULL);

    ssize_t ret = recv_all(sockfd, (void*)&msg->hdr, sizeof(MsgHdr));
    if (ret < 0)
    {
        return -1;
    }

    msg->data = malloc(msg->hdr.length);

    ret = recv_all(sockfd, msg->data, msg->hdr.length);
    if (ret < 0)
    {
        return -1;
    }

    clock_gettime(CLOCK_REALTIME, &msg->rcvd_at);

    return 0;
}
