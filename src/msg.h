#ifndef _PROTO_H
#define _PROTO_H

#include <stddef.h>
#include <uuid/uuid.h>
#include "types.h"

#define MSGTYPES                                    \
    X(MSGTYPE_NONE, "MSGTYPE_NONE")                 \
    X(MSGTYPE_AUTH_REQ, "MSGTYPE_AUTH_REQ")         \
    X(MSGTYPE_AUTH_OK, "MSGTYPE_AUTH_OK")           \
    X(MSGTYPE_AUTH_FAIL, "MSGTYPE_AUTH_FAIL")       \
    X(MSGTYPE_UPLOAD_REQ, "MSGTYPE_UPLOAD_REQ")     \
    X(MSGTYPE_UPLOAD_RES, "MSGTYPE_UPLOAD_RES")     \
    X(MSGTYPE_UPLOAD_FIN, "MSGTYPE_UPLOAD_FIN")     \
    X(MSGTYPE_DOWNLOAD_REQ, "MSGTYPE_DOWNLOAD_REQ") \
    X(MSGTYPE_DOWNLOAD_RES, "MSGTYPE_DOWNLOAD_RES") \
    X(MSGTYPE_DOWNLOAD_FIN, "MSGTYPE_DOWNLOAD_FIN") \
    X(MSGTYPE_LIST_REQ, "MSGTYPE_LIST_REQ")         \
    X(MSGTYPE_LIST_RES, "MSGTYPE_LIST_RES")         \
    X(MSGTYPE_REMOVE_REQ, "MSGTYPE_REMOVE_REQ")     \
    X(MSGTYPE_REMOVE_OK, "MSGTYPE_REMOVE_OK")       \
    X(MSGTYPE_REMOVE_FAIL, "MSGTYPE_REMOVE_FAIL")   \
    X(MSGTYPE_SEND_CHUNK, "MSGTYPE_SEND_CHUNK")     \
    X(MSGTYPE_CHUNK_OK, "MSGTYPE_CHUNK_OK")         \
    X(MSGTYPE_CHUNK_AGAIN, "MSGTYPE_CHUNK_AGAIN")

typedef enum {
#define X(id, name) id,
    MSGTYPES
#undef X
} MessageType;

typedef struct {
    size_t length;
    MessageType type;
    uuid_t token;
    struct timespec sent_at;
} MessageHdr;

typedef struct {
    MessageHdr hdr;
    struct timespec rcvd_at;
    byte* payload;
    size_t payload_len;
} Message;

/**
 * Initialize the Message struct by pointer
 */
void msg_init(Message* self, MessageType type, byte* payload, size_t length);

const char* msg_type_to_str(MessageType type);
void msg_print(Message* self);

/*
 * Send a Message structure on a TCP socket,
 * with an optional payload header for zero-copy
 * sending of payloads which have an header
 * themselves and a pointer to the actual data
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when send returns 0 (which is highly unusual,
 *      but should be treated as a disconnection)
 */
int msg_send(Message* self, int sockfd, byte* payload_hdr, size_t payload_hdr_len);

/*
 * Receive only the message header in an existing Message
 * structure from a TCP socket, to decide what to do based on
 * the type
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when a client gracefully terminates the connection
 */
int msg_recv_header(int sockfd, Message* msg);

/*
 * Receive only the payload in an existing Message structure
 * on which msg_recv_header was already called
 *
 * Warning: Must only be called be called after msg_recv_header()!
 *      on the same message structure
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when a client gracefully terminates the connection
 *  -3 when the len received in the message header is bigger than `maxlen` to avoid DoS attacks
 */
int msg_recv_payload(int sockfd, Message* msg, size_t maxlen);

/*
 * Receive a full Message structure (header and payload)
 * from a TCP socket
 *
 * Returns:
 *   0 on success
 *  -1 when a system error happens (errno is set)
 *  -2 when a client gracefully terminates the connection
 *  -3 when the len received in the message header is bigger than `maxlen` to avoid DoS attacks
 */
int msg_recv(int sockfd, Message* msg, size_t maxlen);

#endif // !_PROTO_H
