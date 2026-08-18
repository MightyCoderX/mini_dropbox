#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#include "user.h"
#include "session.h"

void user_init(User* self, uuid_t token)
{
    if (token == NULL)
    {
        uuid_generate(self->token);
    }
    else
    {
        uuid_copy(self->token, token);
    }
    self->total_space = MAX_USER_SPACE;
    self->used_space = 0;
    self->sessions_count = 0;
    self->sessions_capacity = 10;
    self->sessions = malloc(sizeof(Session*) * self->sessions_capacity);
}

void user_print(User* self)
{
    printf("User:");
    if (self != NULL)
    {
        char token[37];
        uuid_unparse(self->token, token);
        printf("\n  token: %s\n", token);
        printf("  total_space: %zu\n", self->total_space);
        printf("  used_space: %zu\n", self->used_space);
        printf("  sessions: %p\n", (void*)self->sessions);
        printf("  sessions_count: %zu\n", self->sessions_count);
        printf("  sessions_capacity: %zu\n", self->sessions_capacity);
    }
    else
    {
        printf(" NULL\n");
    }
}

int user_add_session(User* self, Session* session)
{
    if (self->sessions_count >= self->sessions_capacity) return -1;

    self->sessions[self->sessions_count++] = session;
    return 0;
}

Session* user_get_session(User* self, SessionType type, const char* filename)
{
    for (size_t i = 0; i < self->sessions_count; i++)
    {
        Session* s = self->sessions[i];
        printf("comparing session filenames arg: %s and info: %s\n", filename,
            s->file_info->filename);
        if (s->type == type && strcmp(s->file_info->filename, filename) == 0)
        {
            return s;
        }
    }

    return NULL;
}

int user_remove_session(User* self, SessionType type, const char* filename)
{
    for (size_t i = 0; i < self->sessions_count; i++)
    {
        Session* s = self->sessions[i];

        if (s->type == type && strcmp(s->file_info->filename, filename) == 0)
        {
            self->sessions[i] = NULL;
            for (size_t j = i; j < self->sessions_count; j++)
            {
                Session* tmp = self->sessions[j];
                self->sessions[j] = self->sessions[j + 1];
                self->sessions[j + 1] = tmp;
            }
            self->sessions_count--;
            return 0;
        }
    }

    return -1;
}

void user_destroy(User* self)
{
    free(self->sessions);
}
