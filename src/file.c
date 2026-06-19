
#include <libgen.h>
#include <math.h>
#include <stddef.h>
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

ssize_t file_send(int sockfd, char* filename)
{
    size_t total_bytes_sent = 0;
    size_t start = 0;
    size_t end = 0;

    FileInfo info;
    int res = fileinfo_from_filename(filename, &info);
    if (res < 0)
    {
        return res;
    }

    int fd = open(filename, O_RDONLY);

    byte buf[CHUNK_SIZE];
    size_t seq = 0;
    while (total_bytes_sent < info.size)
    {
        ssize_t nbytes = read(fd, buf, sizeof(buf));
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
            chunk_send(&chunk, sockfd);
            msg_recv(sockfd, &msg, 0);
        }
        start += nbytes;
        total_bytes_sent += nbytes;
        seq += 1;
    }

    return total_bytes_sent;
}

ssize_t file_recv(int sockfd, FileInfo* info)
{
    size_t total_bytes_recvd = 0;

    int fd = open(info->filename, O_WRONLY | O_CREAT | O_EXCL);
    if (fd < 0)
    {
        return -1;
    }

    Chunk chunk;
    Message msg = { 0 };
    while (total_bytes_recvd < info->size)
    {
        int nbytes = chunk_recv(sockfd, &chunk);
        if (nbytes == -1)
        {
            // generic error
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

        total_bytes_recvd += chunk.hdr.length;
    }

    return total_bytes_recvd;
}
