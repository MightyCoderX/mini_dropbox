#include <stdlib.h>
#include <uuid/uuid.h>

#include "user.h"
#include "user_map.h"
#include "user_manager.h"

UserMap user_map;

void umap_value_free(User* value)
{
    free(value);
}

void uman_init_user_map(void)
{
    umap_init(&user_map, 256);
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

void uman_destroy_user_map(void)
{
    umap_free_entries(&user_map);
}
