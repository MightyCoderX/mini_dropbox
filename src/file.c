#include <libgen.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"
#include "chunk.h"
#include "msg.h"
#include "util.h"

int fileinfo_from_filename(char* filename, FileInfo* out)
{
    struct stat s;
    if (lstat(filename, &s) < 0)
    {
        return -1;
    }

    strcpy(out->filename, filename);
    out->size = s.st_size;
    out->chunk_count = ceil((float)s.st_size / CHUNK_SIZE);
    if (file_checksum(filename, out->checksum) == -1)
    {
        return -1;
    }

    return 0;
}

void fileinfo_print(FileInfo* info)
{
    printf("FileInfo: \n");
    printf("    size: %zu\n", info->size);
    printf("    chunk_count: %zu\n", info->chunk_count);
    printf("    checksum: ");
    checksum_print(info->checksum);
    printf("    filename: %s\n", info->filename);
}

ssize_t file_send(int sockfd, char* filename)
{
    size_t total_bytes_sent = 0;
    size_t start = 0;
    size_t end = 0;

    FileInfo info;
    int res = fileinfo_from_filename(filename, &info);
    if (res == -1)
    {
        return -1;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return -1;
    }

    byte buf[CHUNK_SIZE];
    size_t seq = 0;
    while (seq < info.chunk_count)
    {
        ssize_t nbytes = read(fd, buf, sizeof(buf));
        if (nbytes < 0)
        {
            perror("read");
            return -1;
        }

        if (nbytes == 0)
        {
            return -2;
        }

        end += nbytes - 1;

        Chunk chunk;
        ChunkHdr hdr = {
            .length = nbytes,
            .start_byte = start,
            .end_byte = end,
            .seq = seq,
        };
        chunk_init(&chunk, hdr, buf, nbytes);

        Message msg = { 0 };
        while (msg.hdr.type != MSGTYPE_CHUNK_OK)
        {
            printf("retry sending chunk #%zu\n", seq);
            chunk_send(&chunk, sockfd);
            msg_recv(sockfd, &msg, 0);
        }
        printf("sent chunk #%zu/%zu\n", seq, info.chunk_count);
        start += nbytes;
        total_bytes_sent += nbytes;
        seq++;
    }

    return total_bytes_sent;
}

ssize_t file_recv(int sockfd, FileInfo* info)
{
    size_t total_bytes_recvd = 0;

    int fd = open(info->filename, O_WRONLY | O_CREAT);
    if (fd == -1)
    {
        DEBUG_PRINTF("Failed to open file %s\n", info->filename);
        perror("open");
        return -1;
    }

    Chunk chunk = { 0 };
    Message msg = { 0 };
    size_t seq = 0;
    while (seq < info->chunk_count)
    {
        int nbytes = chunk_recv(sockfd, &chunk);
        if (nbytes == -1)
        {
            // generic error
            perror("chunk_recv");
            return -1;
            continue;
        }

        if (nbytes == -2) // checksum didn't match
        {
            msg_init(&msg, MSGTYPE_CHUNK_AGAIN, NULL, 0);
            msg_send(&msg, sockfd, NULL, 0);
            continue;
        }

        msg_init(&msg, MSGTYPE_CHUNK_OK, NULL, 0);
        msg_send(&msg, sockfd, NULL, 0);

        write(fd, chunk.data, chunk.hdr.length);

        printf("recvd chunk #%zu/%zu\n", seq, info->chunk_count);
        total_bytes_recvd += chunk.hdr.length;
        seq++;
    }

    return total_bytes_recvd;
}
