#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

typedef enum {
    XDG_CONFIG_HOME,
    XDG_STATE_HOME,
} XDGDir;

const char* xdg_dir_to_str(XDGDir dir);
int xdg_get_dir(XDGDir dir, char* out_path, size_t max_len);

int connect_to_server(char* server_ip, short port);

#endif // !UTIL_H
