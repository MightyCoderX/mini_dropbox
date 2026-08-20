#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "server.h"
#include "user.h"
#include "user_map.h"
#include "user_manager.h"

UserMap user_map;

void umap_value_free(User* value)
{
    free(value);
}

void uman_init(void)
{
    umap_init(&user_map, 256);
    DIR* dir = opendir(STORAGE_DIR);
    if (dir == NULL)
    {
        perror("opendir");
        return;
    }

    // TODO: calculate and set storage space and other user data
    printf("Loading users by tokens from directory names in %s\n", STORAGE_DIR);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        uuid_t token;
        if (uuid_parse(entry->d_name, token) == -1)
        {
            printf("invalid UUID %s, skipping user\n", entry->d_name);
        }

        uman_register_user(token);
    }
    printf("Loaded users\n");
}

User* uman_get_user(uuid_t token)
{
    User* user = NULL;
    umap_get(&user_map, token, &user);
    return user;
}

void uman_register_user(uuid_t token)
{
    User* user = malloc(sizeof(*user));

    user_init(user, token);

    umap_put(&user_map, token, user);
}

void uman_deinit(void)
{
    umap_free_entries(&user_map);
}
