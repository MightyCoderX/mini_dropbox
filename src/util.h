#ifndef UTIL_H
#define UTIL_H

#include "types.h"
#include <stddef.h>

typedef enum {
    XDG_CONFIG_HOME,
    XDG_STATE_HOME,
} XDGDir;

const char* xdg_dir_to_str(XDGDir dir);
int xdg_get_dir(XDGDir dir, char* out_path, size_t max_len);

int connect_to_server(char* server_ip, short port);

typedef byte checksum_t[32];

int file_checksum(const char* filename, checksum_t checksum);
u32 checksum(const byte* data, size_t data_len, checksum_t checksum);
void checksum_print(checksum_t checksum);

#endif // !UTIL_H
