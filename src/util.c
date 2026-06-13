#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <openssl/evp.h>
#include <unistd.h>

#include "util.h"
#include "chunk.h"

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

int connect_to_server(char* server_ip, short port)
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
