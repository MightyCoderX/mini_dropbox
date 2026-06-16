#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>

#include "types.h"
#include "user.h"

typedef struct {
    size_t size;
    size_t chunk_count;
    byte checksum[32];
    char name[64];
} FileInfo;
int fileinfo_from_filename(char* filename, FileInfo* out);

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
    User* user;
    FileInfo* file_info;
    SessionType type;
    SessionState state;
    struct timespec started_at;
    size_t chunks_transferred;
} Session;

void session_init(Session* self);
void session_start(Session* self, User* user, FileInfo* file_info, SessionType type);
void session_destroy(Session* self);

#endif // SESSION_H
