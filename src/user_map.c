#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h>

#include "user.h"
#include "types.h"
#include "user_map.h"

void umap_init(UserMap* map, size_t capacity)
{
    map->entries = malloc(capacity * sizeof(UserMapEntry*));
    map->capacity = capacity;
    map->count = 0;

    memset(map->entries, 0, capacity * sizeof(UserMapEntry*));
}

static u64 hash(const byte* str)
{
    u64 hash = 5381;

    int c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

static size_t umap_key_to_index(UserMap* map, uuid_t key)
{
    return hash(key) % map->capacity;
}

int umap_put(UserMap* map, uuid_t key, User* value)
{
    size_t idx = umap_key_to_index(map, key);

    UserMapEntry* new_entry = malloc(sizeof(*new_entry));
    uuid_copy(new_entry->key, key);
    new_entry->value = value;

    // TODO: put actual insertion in one place only instead of in the branches
    if (map->entries[idx] == NULL)
    {
        map->entries[idx] = new_entry;
        map->count++;
        return 0;
    }

    if (uuid_compare(map->entries[idx]->key, key) == 0)
    {
        return -1;
    }

    UserMapEntry* cur = map->entries[idx];
    while (cur->next != NULL)
    {
        cur = cur->next;
    }
    cur->next = new_entry;
    map->count++;

    return 0;
}

int umap_get(UserMap* map, uuid_t key, User** out)
{
    size_t idx = umap_key_to_index(map, key);

    if (map->entries[idx] == NULL)
    {
        return -1;
    }

    if (uuid_compare(map->entries[idx]->key, key) == 0)
    {
        *out = map->entries[idx]->value;
        return 0;
    }

    UserMapEntry* cur = map->entries[idx];
    while (cur != NULL)
    {
        if (uuid_compare(cur->key, key) == 0)
        {
            *out = map->entries[idx]->value;
            return 0;
        }
        cur = cur->next;
    }

    return -1;
}

void umap_entry_free(UserMapEntry* entry)
{
    UserMapEntry* cur = entry;
    while (cur->next != NULL)
    {
        UserMapEntry* next = cur->next;
        umap_value_free(cur->value);
        free(cur);
        cur = next;
    }
    free(cur);
}

void umap_free_entries(UserMap* map)
{
    for (size_t i = 0; i < map->capacity; i++)
    {
        free(map->entries[i]);
    }
    free(map->entries);
}
