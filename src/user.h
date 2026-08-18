#ifndef USER_H
#define USER_H

#include <stddef.h>
#include <uuid/uuid.h>

#include "session.h"

#define MAX_USER_SPACE 10e9 // 10 GB

typedef struct {
    uuid_t token;
    size_t total_space;
    size_t used_space;
    Session** sessions;
    size_t sessions_count;
    size_t sessions_capacity;
} User;

void user_init(User* self, uuid_t token);

void user_print(User* self);

int user_add_session(User* self, Session* session);
Session* user_get_session(User* self, SessionType type, const char* filename);
int user_remove_session(User* self, SessionType type, const char* filename);

void user_destroy(User* self);

#endif // USER_H
