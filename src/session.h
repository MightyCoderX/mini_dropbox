#ifndef SESSION_H
#define SESSION_H

#include "user.h"
#include <stddef.h>
typedef struct {
    char* name;
    size_t size;
    size_t chunk_count;
} FileInfo;

typedef struct {
    User user;
    FileInfo file;
    size_t bytes_transfered;
    size_t current_chunk;
} Session;

void session_init(Session* self, User* user);

#endif // SESSION_H
