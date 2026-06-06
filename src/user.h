#ifndef USER_H
#define USER_H

#include <stddef.h>
#include <uuid/uuid.h>

#define MAX_USER_SPACE 10e9 // 10 GB

typedef struct {
    uuid_t token;
    size_t total_space;
    size_t used_space;
} User;

void user_init(User* self);

#endif // USER_H
