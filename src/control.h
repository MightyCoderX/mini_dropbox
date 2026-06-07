
#define MSGTYPES                    \
    X(AUTH_REQ, "AUTH_REQ")         \
    X(AUTH_OK, "AUTH_OK")           \
    X(AUTH_FAIL, "AUTH_FAIL")       \
    X(UPLOAD_REQ, "UPLOAD_REQ")     \
    X(UPLOAD_RES, "UPLOAD_RES")     \
    X(UPLOAD_FIN, "UPLOAD_FIN")     \
    X(DOWNLOAD_REQ, "DOWNLOAD_REQ") \
    X(DOWNLOAD_RES, "DOWNLOAD_RES") \
    X(DOWNLOAD_FIN, "DOWNLOAD_FIN") \
    X(LIST_REQ, "LIST_REQ")         \
    X(LIST_RES, "LIST_RES")         \
    X(REMOVE_REQ, "REMOVE_REQ")     \
    X(REMOVE_OK, "REMOVE_OK")       \
    X(REMOVE_FAIL, "REMOVE_FAIL")   \
    X(SEND_CHUNK, "SEND_CHUNK")     \
    X(CHUNK_OK, "CHUNK_OK")         \
    X(CHUNK_AGAIN, "CHUNK_AGAIN")

typedef enum {
#define X(id, name) id,
    MSGTYPES
#undef X
} MessageType;
