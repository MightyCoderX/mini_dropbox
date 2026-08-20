#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>

#include <linux/limits.h>
#include <time.h>

#include "file.h"

typedef enum {
    SESS_NONE,
    SESS_DOWNLOAD,
    SESS_UPLOAD,
} SessionType;

typedef enum {
    SSTATE_IDLE,
    SSTATE_RUNNING,
    SSTATE_INTERRUPTED,
} SessionState;

typedef struct {
    SessionType type;
    SessionState state;
    FileInfo* file_info;
    size_t last_transfered_chunk;
    struct timespec started_at;
} Session;

void session_init(Session* self, FileInfo* file_info, SessionType type);

const char* sesstype_to_str(SessionType type);
const char* sstate_to_str(SessionState state);

void session_print(Session* self);

#endif // SESSION_H
