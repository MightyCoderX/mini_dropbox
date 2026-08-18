#include <assert.h>
#include <libgen.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
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
        perror("lstat");
        return -1;
    }

    if ((s.st_mode & S_IFMT) != S_IFREG)
    {
        return -2;
    }

    strcpy(out->filename, filename);
    out->size = s.st_size;
    out->chunk_count = ceil((float)s.st_size / CHUNK_SIZE);
    if (file_checksum(filename, out->checksum) == -1)
    {
        return -3;
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

FileSendStats file_send(int sockfd, char* filename, size_t start_chunk)
{
    FileInfo info;
    int res = fileinfo_from_filename(filename, &info);
    if (res == -1)
    {
        return (FileSendStats) { 0, -1 };
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return (FileSendStats) { 0, -1 };
    }

    size_t start = 0;
    size_t end = 0;

    byte buf[CHUNK_SIZE];
    size_t seq = 0;
    if (start_chunk > 0)
    {
        lseek(fd, start_chunk * CHUNK_SIZE, SEEK_SET);
        start = start_chunk * CHUNK_SIZE;
        seq = start_chunk;
    }
    while (seq < info.chunk_count)
    {
        ssize_t nbytes = read(fd, buf, sizeof(buf));
        if (nbytes == -1)
        {
            perror("read");
            return (FileSendStats) { seq + 1, -1 };
        }

        if (nbytes == 0)
        {
            return (FileSendStats) { seq + 1, -1 };
        }

        end = start + nbytes - 1;

        Chunk chunk;
        ChunkHdr hdr = {
            .length = nbytes,
            .start_byte = start,
            .end_byte = end,
            .seq = seq,
        };
        chunk_init(&chunk, hdr, buf);

        Message msg = { 0 };
        while (msg.hdr.type != MSGTYPE_CHUNK_OK)
        {
            printf("sending chunk #%zu\n", seq + 1);

            res = chunk_send(&chunk, sockfd);
            if (res < 0)
            {
                printf("failed to send chunk #%zu\n", seq + 1);
                return (FileSendStats) { seq + 1, -5 };
            }

            res = msg_recv(sockfd, &msg, 0);
            if (res < 0)
            {
                printf("failed to recv OK or AGAIN: res=%d\n", res);
                return (FileSendStats) { seq + 1, -6 };
            }
        }
        printf("sent chunk #%zu/%zu\n", seq + 1, info.chunk_count);

        start += nbytes;
        seq++;
    }

    return (FileSendStats) { seq, 0 };
}

FileRecvStats file_recv(int sockfd, FileInfo* info)
{
    int fd = open(info->filename, O_WRONLY | O_CREAT, 0644);
    if (fd == -1)
    {
        DEBUG_PRINTF("Failed to open file %s\n", info->filename);
        perror("open");
        return (FileRecvStats) { 0, -1 };
    }

    Chunk chunk = { 0 };
    Message msg = { 0 };
    size_t seq = 0;
    while (seq < info->chunk_count)
    {
        int nbytes = chunk_recv(sockfd, &chunk);

        if (nbytes == -3) // checksum didn't match
        {
            DEBUG_PRINTF("checksum didn't match\n");

            msg_init(&msg, MSGTYPE_CHUNK_AGAIN, NULL, 0);

            int res = msg_send(&msg, sockfd, NULL, 0);
            if (res < 0) return (FileRecvStats) { seq, res };

            chunk_free(&chunk);

            continue;
        }

        if (nbytes < 0)
        {
            if (nbytes == -1)
            {
                perror("chunk_recv");
            }
            return (FileRecvStats) { seq, nbytes };
        }

        if (seq == 0 && chunk.hdr.seq > 0)
        {
            printf("recv: resuming transfer\n");
            seq = chunk.hdr.seq;
            off_t ret = lseek(fd, seq * CHUNK_SIZE, SEEK_SET);
            if (ret == -1)
            {
                perror("lseek");
                return (FileRecvStats) { seq, -3 };
            }
        }

        if (seq != chunk.hdr.seq)
        {
            printf("Expected seq=%zu, got seq=%zu\n", seq, chunk.hdr.seq);
            return (FileRecvStats) { seq, -4 };
        }

        msg_init(&msg, MSGTYPE_CHUNK_OK, NULL, 0);
        int res = msg_send(&msg, sockfd, NULL, 0);
        if (res < 0) return (FileRecvStats) { seq, res };

        write(fd, chunk.data, chunk.hdr.length);

        printf("recvd chunk #%zu/%zu\n", seq + 1, info->chunk_count);
        chunk_free(&chunk);
        seq++;
    }

    return (FileRecvStats) { seq, 0 };
}
