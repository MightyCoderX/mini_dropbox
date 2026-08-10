#ifndef UTIL_H
#define UTIL_H

#include "msg.h"
#include "types.h"
#include <stddef.h>

#ifndef RELEASE
#define DEBUG_PRINTF(fmt, ...) fprintf(stderr, "[DEBUG] " fmt, ##__VA_ARGS__);
#else
#define DEBUG_PRINTF(fmt, ...)
#endif

typedef enum {
    XDG_CONFIG_HOME,
    XDG_STATE_HOME,
} XDGDir;

const char* xdg_dir_to_str(XDGDir dir);
int xdg_get_dir(XDGDir dir, char* out_path, size_t max_len);

int connect_to_server(char* server_ip, u16 port);

typedef byte checksum_t[32];

int file_checksum(const char* filename, checksum_t checksum);
u32 checksum(const byte* data, size_t data_len, checksum_t checksum);
bool checksums_match(checksum_t chk1, checksum_t chk2);
void checksum_print(checksum_t checksum);

char* normalize_path(const char* path);
int stridx(const char* str, char c);
char* path_next_dir(const char* full_path, int skip_chars);
int create_directories_from_path(char* root_dir, char* user_path);

int send_error(int sockfd, char* text);

int send_upload_res(int sockfd, byte* payload, size_t len);
int send_download_res(int sockfd, byte* payload, size_t len);

int rm_r(const char* path);

bool handle_recv_error(int ret, MessageType type);

int get_user_root(uuid_t token, char* out);
char* get_user_path(char* path, uuid_t token);

#endif // !UTIL_H
