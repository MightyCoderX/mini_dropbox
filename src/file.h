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

ssize_t file_send(int sockfd, char* filename);
ssize_t file_recv(int sockfd, FileInfo* info);

#endif // !FILE_H
