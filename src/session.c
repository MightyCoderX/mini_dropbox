#include <bits/time.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "session.h"
#include "chunk.h"
#include "util.h"

int fileinfo_from_filename(char* filename, FileInfo* out)
{
    struct stat s;
    if (lstat(filename, &s) < 0)
    {
        return -1;
    }

    strcpy(out->name, filename);
    out->size = s.st_size;
    out->chunk_count = ceil((float)s.st_size / CHUNK_SIZE);
    if (file_checksum(filename, out->checksum) == -1)
    {
        return -1;
    }

    return 0;
}

void session_init(Session* self)
{
    self->user = NULL;
    self->file_info = NULL;
    self->type = SESS_NONE;
    self->state = SSTATE_IDLE;
    self->started_at = (struct timespec) { 0, 0 };
    self->chunks_transferred = 0;
}

void session_start(Session* self, User* user, FileInfo* file_info, SessionType type)
{
    self->user = user;
    self->file_info = file_info;
    self->type = type;
    clock_gettime(CLOCK_REALTIME, &self->started_at);
}
