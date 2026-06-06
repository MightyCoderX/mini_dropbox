#include <asm-generic/errno-base.h>
#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <uuid/uuid.h>

#include "msg.h"
#include "types.h"
#include "user.h"
#include "util.h"

#define streq(str1, str2) (strcmp(str1, str2) == 0)

typedef struct {
    const char* name;
    const char* usage;
    int (*func)(char* progname, int argc, char** argv);
} Command;

// $ client help [cmd]
static int cmd_help(char* progname, int argc, char** argv);

// $ client auth <server_address> # generates a token, saves it locally, and sends it to the server
static int cmd_auth(char* progname, int argc, char** argv);

// $ client upload <server_address> <local_file> <remote_file> # sends file with token attached
static int cmd_upload(char* progname, int argc, char** argv);

// $ client download <server_address> <remote_file> [local_file] # downloads
static int cmd_download(char* progname, int argc, char** argv);

// $ client list <server_address> [path] # downloads
static int cmd_list(char* progname, int argc, char** argv);

// $ client rm <server_address> <remote_file>
static int cmd_rm(char* progname, int argc, char** argv);

#define COMMANDS                                                                         \
    X(CMD_HELP, "help", "[command]", cmd_help)                                           \
    X(CMD_AUTH, "auth", "<server_address>", cmd_auth)                                    \
    X(CMD_DLOD, "upload", "<server_address> <local_file>  [remote_file]", cmd_upload)    \
    X(CMD_ULOD, "download", "<server_address> <remote_file> [local_file]", cmd_download) \
    X(CMD_ULOD, "list", "<server_address> [path]", cmd_list)                             \
    X(CMD_RMFI, "rm", "<server_address> <remote_file>", cmd_rm)

enum {
#define X(id, name, usage, func) id,
    COMMANDS
#undef X
};

static Command commands[] = {
#define X(id, name, usage, func) { name, usage, func },
    COMMANDS
#undef X
};

static Command* cmd_get(const char* name)
{
    if (streq(name, commands[0].name)) return &commands[0];
#define X(id, n, usage, func) else if (streq(name, n)) return &commands[id];
    COMMANDS
#undef X

    return NULL;
}

static void print_help(char* progname);

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        print_help(argv[0]);
        return 0;
    }

    Command* cmd = cmd_get(argv[1]);

    if (!cmd)
    {
        print_help(argv[0]);
        return 1;
    }

    return cmd->func(argv[0], argc - 2, argv + 2);
}

static void print_help(char* progname)
{
    fprintf(stderr, "\nUsage: %s COMMAND\n\n", progname);
    size_t max_len = strlen(commands[0].name);

    size_t cmd_count = sizeof(commands) / sizeof(*commands);

    fprintf(stderr, "Commands:\n");
    for (size_t i = 0; i < cmd_count; i++)
    {
        size_t cmd_len = strlen(commands[i].name);
        if (cmd_len > max_len)
        {
            max_len = cmd_len;
        }
    }

    size_t col_width = (max_len + 2);

    for (size_t i = 0; i < cmd_count; i++)
    {
        size_t spaces = col_width - strlen(commands[i].name);

        fprintf(stderr, "    %s", commands[i].name);

        for (size_t j = 0; j < spaces; j++)
        {
            fprintf(stderr, " ");
        }

        fprintf(stderr, "%s\n", commands[i].usage);
    }
    fprintf(stderr, "\n");
}

int cmd_help(char* progname, int argc, char** argv)
{
    if (argc < 1)
    {
        print_help(progname);
        return 1;
    }

    Command* cmd = cmd_get(argv[0]);
    if (!cmd)
    {
        fprintf(stderr, "Error: invalid command '%s'\n", argv[0]);
        print_help(progname);
        return 1;
    }

    fprintf(stderr, "Usage: %s %s %s\n", progname, cmd->name, cmd->usage);

    return 0;
}

int cmd_auth(char* progname, int argc, char** argv)
{
    if (argc < 1)
    {
        cmd_help(progname, 1, argv - 1);
        return 1;
    }

    User user = { 0 };
    user_init(&user);

    char tokfile_path[PATH_MAX];
    int len = xdg_get_dir(XDG_STATE_HOME, tokfile_path, sizeof(tokfile_path));
    if (len == -1)
    {
        fprintf(stderr, "HOME environment variable not set, cannot save token!\n");
        return 1;
    }

    size_t chars_left = sizeof(tokfile_path) - len;
    strncat(tokfile_path, "/minidropbox.token", chars_left);

    int fd = open(tokfile_path, O_CREAT | O_WRONLY | O_EXCL, 0755);
    if (fd == -1 && errno != EEXIST)
    {
        perror("open");
        return 1;
    }

    char token[37];
    if (errno == EEXIST)
    {
        fd = open(tokfile_path, O_RDONLY);
        if (fd == -1)
        {
            perror("open");
            return 1;
        }
        read(fd, token, sizeof(token));
        uuid_parse(token, user.token);
        fprintf(stderr, "Read token from file %s\n", tokfile_path);
    }
    else
    {
        uuid_unparse(user.token, token);
        write(fd, token, sizeof(token));
        fprintf(stderr, "Wrote token to file %s\n", tokfile_path);
    }

    Message msg = { 0 };
    msg_init(&msg, AUTH_REQ, user.token, sizeof(user.token));

    int sockfd = connect_to_server(argv[1], 1234);
    if (sockfd == -1)
    {
        perror("connect_to_server");
        return 1;
    }

    ssize_t res = msg_send(&msg, sockfd);
    if (res == -1)
    {
        perror("msg_send");
        return 1;
    }

    return 0;
}

// $ client upload <server_address> <local_file> <remote_file> # sends file with token attached
static int cmd_upload(char* progname, int argc, char** argv)
{
    // TODO: 1. send UPLOAD_REQ message with token (if token not found, prompt the user to run auth and return 1)
    // TODO: 2. if not ack (ex. user has no more space) print error and return 1
    // TODO: 3. calculate file checksum
    // TODO: 4. calculate # of chunks needed
    // TODO: 5. send chunks
    //         - calculate chunk checksum (parallel to the sending using a 2 threads pool)
    //         - send chunk
    //         - wait for ack
    //         - resend if ack says that remote checksum was different
    // TODO: 6. send UPLOAD_FIN message
    return 0;
}

// $ client download <server_address> <remote_file> [local_file] # downloads
static int cmd_download(char* progname, int argc, char** argv)
{
    // TODO: 1. send DOWNLOAD_REQ message with token (if token not found, prompt the user to run auth and return 1)
    // TODO: 2. if not ack (ex. user has no more space) print error and return 1
    // TODO: 3. get FileInfo from ack message
    // TODO: 4. receive chunks
    //          - calculate local chunk checksum
    //          - compare with received one
    //          -
    // TODO: 5. check
    // TODO: 5. send DOWNLOAD_FIN when download finished
    return 0;
}

// $ client rm <server_address> <remote_file>
static int cmd_rm(char* progname, int argc, char** argv)
{
    return 0;
}
