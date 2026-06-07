#ifndef _PROTO_H
#define _PROTO_H

#include <stddef.h>
#include <uuid/uuid.h>
#include "types.h"

#define MSGTYPES          \
    X(CONTROL, "CONTROL") \
    X(DATA, "DATA")

typedef enum {
#define X(id, name) id,
    MSGTYPES
#undef X
} MessageType;

typedef struct {
    size_t length;
    MessageType type;
    struct timespec sent_at;
} MessageHdr;

typedef struct {
    MessageHdr hdr;
    struct timespec rcvd_at;
    byte* data;
} Message;

void msg_init(Message* self, MessageType type, byte* data, size_t length);

ssize_t msg_send(Message* self, int sockfd);
ssize_t msg_recv(int sockfd, Message* msg);

const char* msg_type_to_str(MessageType type);

#endif // !_PROTO_H
