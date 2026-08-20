#ifndef _USER_MANAGER_H
#define _USER_MANAGER_H

#include <uuid/uuid.h>

#include "user_map.h"

extern UserMap user_map;

void uman_init(void);
User* uman_get_user(uuid_t token);
void uman_register_user(uuid_t token);
void uman_deinit(void);

#endif // !_USER_MANAGER_H
