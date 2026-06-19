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
bool checksums_match(checksum_t chk1, checksum_t chk2);
void checksum_print(checksum_t checksum);

char* normalize_path(const char* path);
int stridx(const char* str, char c);
char* path_next_dir(const char* full_path, int skip_chars);
int create_directories_from_path(char* root_dir, char* user_path);

#endif // !UTIL_H
