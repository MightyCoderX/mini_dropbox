#ifndef FILE_H
#define FILE_H

#include <stddef.h>
#include <stdio.h>

#include <linux/limits.h>

#include "types.h"

typedef struct {
    size_t size;
    size_t chunk_count;
    byte checksum[32];
    char filename[PATH_MAX];
} FileInfo;
int fileinfo_from_filename(char* filename, FileInfo* out);
void fileinfo_print(FileInfo* info);

typedef struct {
    size_t last_chunk_sent;
    ssize_t error_code;
} FileSendStats;
FileSendStats file_send(int sockfd, char* filename, size_t start_chunk);

typedef struct {
    size_t last_chunk_recvd;
    ssize_t error_code;
} FileRecvStats;
FileRecvStats file_recv(int sockfd, FileInfo* info);

#endif // !FILE_H
