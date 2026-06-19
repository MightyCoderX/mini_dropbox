#include "session.h"

void session_init(Session* self, User* user, FileInfo* file_info, SessionType type)
{
    self->type = type;
    self->state = SSTATE_IDLE;
    self->user = user;
    self->file_info = file_info;
    clock_gettime(CLOCK_REALTIME, &self->started_at);
    self->chunks_transferred = 0;
}
