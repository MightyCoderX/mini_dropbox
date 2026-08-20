#include "session.h"
#include <stdio.h>

void session_init(Session* self, FileInfo* file_info, SessionType type)
{
    self->type = type;
    self->state = SSTATE_IDLE;
    self->file_info = file_info;
    clock_gettime(CLOCK_REALTIME, &self->started_at);
    self->last_transfered_chunk = 0;
}

const char* sesstype_to_str(SessionType type)
{
    switch (type)
    {
    case SESS_NONE:
        return "SESS_NONE";
    case SESS_DOWNLOAD:
        return "SESS_DOWNLOAD";
    case SESS_UPLOAD:
        return "SESS_UPLOAD";
    }

    return "INVALID";
}

const char* sstate_to_str(SessionState state)
{
    switch (state)
    {
    case SSTATE_IDLE:
        return "SSTATE_IDLE";
    case SSTATE_RUNNING:
        return "SSTATE_RUNNING";
    case SSTATE_INTERRUPTED:
        return "SSTATE_INTERRUPTED";
    }

    return "INVALID";
}

void session_print(Session* self)
{
    printf("Session:");
    if (self != NULL)
    {
        printf("\n");
        printf("  type: %s\n", sesstype_to_str(self->type));
        printf("  state: %s\n", sstate_to_str(self->state));
        printf("  filename: %s\n", self->file_info->filename);
        printf("  chunks_transferred: %zu\n", self->last_transfered_chunk);
        printf("  started_at: %zus %zuns\n", self->started_at.tv_sec, self->started_at.tv_nsec);
    }
    else
    {
        printf(" NULL\n");
    }
}
