#ifndef _PROTO_H
#define _PROTO_H

#include <stddef.h>
#include <uuid/uuid.h>
#include "types.h"

typedef enum {
    AUTH_REQ,
    AUTH_OK,
    AUTH_FAIL,
    UPLOAD_REQ,
    UPLOAD_RES,
    UPLOAD_FIN,
    DOWNLOAD_REQ,
    DOWNLOAD_RES,
    DOWNLOAD_FIN,
    LIST_REQ,
    LIST_RES,
    SEND_CHUNK,
    CHUNK_OK,
    CHUNK_AGAIN,
} MessageType;

typedef struct {
    size_t length;
    MessageType type;
    struct timespec sent_at;
} MsgHdr;

typedef struct {
    MsgHdr* hdr;
    struct timespec rcvd_at;
    byte* data;
} Message;

void msg_init(Message* self, MessageType type, byte* data, size_t length);

ssize_t msg_send(Message* self, int sockfd);
ssize_t msg_recv(int sockfd, Message* msg);

#endif // !_PROTO_H
