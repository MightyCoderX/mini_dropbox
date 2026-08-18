#ifndef _USER_MAP_H
#define _USER_MAP_H

#include <stddef.h>
#include <uuid/uuid.h>

#include "user.h"

typedef struct UserMapEntry UserMapEntry;

typedef struct {
    UserMapEntry** entries;
    size_t count;
    size_t capacity;
} UserMap;

struct UserMapEntry {
    uuid_t key;
    User* value;
    UserMapEntry* next;
};

void umap_init(UserMap* map, size_t capacity);

int umap_put(UserMap* map, uuid_t key, User* value);
int umap_get(UserMap* map, uuid_t key, User** out);

extern void umap_value_free(User* value);
void umap_entry_free(UserMapEntry* entry);
void umap_free_entries(UserMap* map);

#endif // !_USER_MAP_H
