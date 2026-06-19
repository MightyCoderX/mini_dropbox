#include "session.h"
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
