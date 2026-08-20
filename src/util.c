#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/evp.h>
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <libgen.h>

#include "util.h"
#include "chunk.h"
#include "msg.h"
#include "server.h"
#include "types.h"

const char* xdg_dir_to_str(XDGDir dir)
{
    switch (dir)
    {
    case XDG_CONFIG_HOME:
        return "XDG_CONFIG_HOME";
    case XDG_STATE_HOME:
        return "XDG_STATE_HOME";
    default:
        assert(0 && "Unreachable");
    }
}

int xdg_get_dir(XDGDir dir, char* out_path, size_t max_len)
{
    const char* xdg_env = getenv(xdg_dir_to_str(dir));

    if (xdg_env && *xdg_env != '\0')
    {
        return snprintf(out_path, max_len, "%s", xdg_env);
    }

    const char* home_env = getenv("HOME");

    if (home_env && *home_env != '\0')
    {
        return snprintf(out_path, max_len, "%s/.config", home_env);
    }

    return -1;
}

int connect_to_server(char* server_ip, u16 port)
{
    assert(server_ip != NULL);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    /*
     * Configure address
     */
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    // Convert ip address to int
    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
    {
        fprintf(stderr, "invalid or unsupported address '%s'\n", server_ip);
        return -1;
    }

    /*
     * Connect to the server
     */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        return -1;
    }

    return sockfd;
}

u32 checksum(const byte* data, size_t data_len, checksum_t checksum)
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, data, data_len);
    u32 checksum_len;
    EVP_DigestFinal_ex(ctx, checksum, &checksum_len);
    EVP_MD_CTX_free(ctx);

    return checksum_len;
}

int file_checksum(const char* filename, checksum_t checksum)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        close(fd);
        return -1;
    }

    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);

    byte buffer[CHUNK_SIZE];
    size_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0)
    {
        EVP_DigestUpdate(ctx, buffer, bytes_read);
    }

    u32 md_len;

    EVP_DigestFinal_ex(ctx, (void*)checksum, &md_len);

    EVP_MD_CTX_free(ctx);
    close(fd);
    return 0;
}

void checksum_print(checksum_t checksum)
{
    for (size_t i = 0; i < sizeof(checksum_t); i++)
    {
        printf("%02x", checksum[i]);
    }
    printf("\n");
}

bool checksums_match(checksum_t chk1, checksum_t chk2)
{
    for (size_t i = 0; i < sizeof(checksum_t); i++)
    {
        if (chk1[i] != chk2[i])
        {
            return false;
        }
    }

    return true;
}

char* normalize_path(const char* path)
{
    if (!path) return NULL;

    char* out = malloc(strlen(path) + 2);
    if (!out) return NULL;

    const char* p = path;
    int i = 0;
    int is_absolute = (*p == '/');

    if (is_absolute) out[i++] = '/';

    while (*p)
    {
        if (*p == '/')
        {
            p++;
            continue;
        }

        const char* start = p;
        while (*p && *p != '/')
            p++;
        int len = p - start;

        if (len == 1 && start[0] == '.')
        {
            // skip .
        }
        else if (len == 2 && start[0] == '.' && start[1] == '.')
        {
            if (is_absolute)
            {
                // don't go past root
                if (i > 1)
                {
                    i--;
                    while (i > 1 && out[i - 1] != '/')
                        i--;
                }
            }
            else
            {
                // can we go up?
                if (i > 0 && !(out[i - 1] == '.' && (i < 2 || out[i - 2] == '/')))
                {
                    // rewind past last component
                    i--;
                    while (i > 0 && out[i - 1] != '/')
                        i--;
                    if (i > 0) i--; // remove trailing slash
                }
                else
                {
                    // no component to rewind — keep the ..
                    if (i > 0) out[i++] = '/';
                    memcpy(out + i, "..", 2);
                    i += 2;
                }
            }
        }
        else
        {
            if (i > 0 && out[i - 1] != '/') out[i++] = '/';
            memcpy(out + i, start, len);
            i += len;
        }
    }

    if (i == 0)
    {
        out[i++] = '.';
    }
    out[i] = '\0';
    return out;
}

int stridx(const char* str, char c)
{
    char* p = strchr(str, c);
    if (p == NULL) return -1;

    return str - p;
}

char* path_next_dir(const char* full_path, int skip_chars)
{
    static char* path;
    static int len;
    static int curr_slash;
    if (full_path != NULL)
    {
        path = strdup(full_path);
        len = strlen(path);
        curr_slash = -1;
    }

    if (curr_slash >= len)
    {
        return NULL;
    }

    if (curr_slash >= 0 && curr_slash < len) path[curr_slash] = '/';

    int i = curr_slash + 1;

    while ((path[i] != '/' || i <= skip_chars) && path[i] != '\0')
    {
        i++;
    }
    path[i] = '\0';
    curr_slash = i;

    return path;
}

int create_directories_from_path(char* root_dir, char* user_path)
{
    char abs_path[PATH_MAX];
    snprintf(abs_path, sizeof(abs_path), "%s/%s", root_dir, user_path);

    char* path = normalize_path(abs_path);
    if (path == NULL)
    {
        return -1;
    }

    size_t root_dir_len = strlen(root_dir);
    if (strncmp(root_dir, path, root_dir_len) != 0)
    {
        free(path);
        return -2;
    }

    user_path = &path[root_dir_len];

    char* dir = path_next_dir(path, root_dir_len - 1);

    while ((dir = path_next_dir(NULL, 0)) != NULL)
    {
        if (mkdir(dir, 0700) < 0)
        {
            if (errno != EEXIST)
            {
                return -3;
            }
        }
    }

    free(path);
    return 0;
}

int send_error(int sockfd, char* text)
{
    Message msg = { 0 };
    msg_init(&msg, MSGTYPE_ERROR, (void*)text, strlen(text) + 1);
    return msg_send(&msg, sockfd, NULL, 0);
}

int send_upload_res(int sockfd, byte* payload, size_t len)
{
    Message msg = { 0 };
    msg_init(&msg, MSGTYPE_UPLOAD_RES, payload, len);
    return msg_send(&msg, sockfd, NULL, 0);
}

int send_download_res(int sockfd, byte* payload, size_t len)
{
    Message msg = { 0 };
    msg_init(&msg, MSGTYPE_DOWNLOAD_RES, payload, len);
    return msg_send(&msg, sockfd, NULL, 0);
}

int rm_r(const char* path)
{
    if (path == NULL)
    {
        return -1;
    }

    struct stat s;
    if (lstat(path, &s) < 0)
    {
        perror("lstat");
        return -1;
    }

    if ((s.st_mode & S_IFMT) != S_IFDIR)
    {
        return unlink(path);
    }

    DIR* dir = opendir(path);
    if (dir == NULL)
    {
        perror("opendir");
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char filepath[PATH_MAX];
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        rm_r(filepath);
    }

    if (rmdir(path) < 0)
    {
        perror("rmdir");
        return -1;
    }
    return 0;
}

bool handle_recv_error(int ret, MessageType type)
{
    if (ret >= 0) return true;

    switch (ret)
    {
    case -1:
        printf("error when receiving %s message: ret = %d, errno = %d (%s)\n",
            msg_type_to_str(type), ret, errno, strerror(errno));
        break;
    case -2:
        printf("peer disconnected gracefully while receiving %s: ret = %d\n", msg_type_to_str(type),
            ret);
        break;
    case -3:
        printf("received payload bigger than max\n");
        break;
    }

    return false;
}

int get_user_root(uuid_t token, char* out)
{
    char tokenstr[37];
    uuid_unparse(token, tokenstr);
    snprintf(out, PATH_MAX, "%s/%s", STORAGE_DIR, tokenstr);
    return 0;
}

char* get_user_path(char* path, uuid_t token)
{
    char root_dir[PATH_MAX];
    get_user_root(token, root_dir);
    char abs_path[PATH_MAX * 2];
    snprintf(abs_path, sizeof(abs_path), "%s/%s", root_dir, path);
    char* normalized = normalize_path(abs_path);
    if (normalized == NULL)
    {
        DEBUG_PRINTF("Failed to normalize path '%s'\n", path);
        return NULL;
    }

    if (strncmp(normalized, root_dir, strlen(root_dir)) != 0)
    {
        // user tried a path traversal attack
        printf("client sent invalid path %s\n", path);
        return NULL;
    }

    return normalized;
}

int file_exists(const char* filename)
{
    errno = 0;
    int fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        if (errno != ENOENT)
        {
            perror("open");
            return -1;
        }

        return 0;
    }

    close(fd);
    return 1;
}
