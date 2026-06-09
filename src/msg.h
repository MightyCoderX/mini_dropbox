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
    byte* payload;
    size_t payload_len;
} Message;

/**
 * Initialize the Message struct
 */
void msg_init(Message* self, MessageType type, byte* payload, size_t length);

/*
 * Send a Message structure on a TCP socket,
 * with an optional payload header for zero-copy
 * sending of payloads which have an header
 * themselves and a pointer to the actual data
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when send returns 0 (which is highly unusual, but should be treated as a disconnection)
 */
int msg_send(Message* self, int sockfd, byte* payload_hdr, size_t payload_hdr_len);

/*
 * Recieve a Message structure from a TCP socket
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when a client gracefully terminates the connection
 *  -3 when the len received in the message header is bigger than `maxlen` to avoid DoS attacks
 */
int msg_recv(int sockfd, Message* msg, size_t maxlen);

const char* msg_type_to_str(MessageType type);

#endif // !_PROTO_H
