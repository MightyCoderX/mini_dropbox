#include <uuid/uuid.h>

#include "user.h"

void user_init(User* self)
{
    uuid_generate(self->token);
    self->total_space = MAX_USER_SPACE;
    self->used_space = 0;
}
