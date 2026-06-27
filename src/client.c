#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <uuid/uuid.h>

#include "file.h"
#include "msg.h"
#include "util.h"

#define DEFAULT_SERVER_IP "127.0.0.1"

#define streq(str1, str2) (strcmp(str1, str2) == 0)

#define COMMANDS                                                        \
    X(CMD_HELP, "help", "[command]", cmd_help)                          \
    X(CMD_AUTH, "auth", "", cmd_auth)                                   \
    X(CMD_ULOD, "upload", "<local_file> [remote_file]", cmd_upload)     \
    X(CMD_DLOD, "download", "<remote_file> [local_file]", cmd_download) \
    X(CMD_LIST, "list", "[path]", cmd_list)                             \
    X(CMD_RMFI, "rm", "<remote_file>", cmd_rm)

typedef enum {
#define X(id, name, usage, func) id,
    COMMANDS
#undef X
} CommandID;

typedef struct {
    CommandID id;
    const char* name;
    const char* usage;
    int (*func)(char* progname, int argc, char** argv);
} Command;

static int cmd_help(char* progname, int argc, char** argv);
static int cmd_auth(char* progname, int argc, char** argv);
static int cmd_upload(char* progname, int argc, char** argv);
static int cmd_download(char* progname, int argc, char** argv);
static int cmd_list(char* progname, int argc, char** argv);
static int cmd_rm(char* progname, int argc, char** argv);

static Command commands[] = {
#define X(id, name, usage, func) { id, name, usage, func },
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
void print_cmd_usage(char* progname, CommandID id);

static char server_ip[INET_ADDRSTRLEN] = DEFAULT_SERVER_IP;

int main(int argc, char** argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "s:")) != -1)
    {
        switch (opt)
        {
        case 's':
            strcpy(server_ip, optarg);
            break;
        default:
            print_help(argv[0]);
            return 1;
        }
    }

    if (optind >= argc)
    {
        print_help(argv[0]);
        return 1;
    }

    Command* cmd = cmd_get(argv[optind]);

    if (!cmd)
    {
        print_help(argv[0]);
        return 1;
    }

    switch (cmd->id)
    {
    case CMD_HELP:
        break;
    case CMD_AUTH:
    case CMD_DLOD:
    case CMD_ULOD:
    case CMD_LIST:
    case CMD_RMFI:
        // TODO: ideally open socket here for commands that need it
        break;
    }
    return cmd->func(argv[0], argc - (optind + 1), &argv[optind + 1]);
}

static void print_help(char* progname)
{
    fprintf(stderr, "\nusage: %s [-s SERVER_IP] COMMAND\n\n", progname);
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

void print_cmd_usage(char* progname, CommandID id)
{
    Command cmd = commands[id];
    fprintf(stderr, "usage: %s %s %s\n", progname, cmd.name, cmd.usage);
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

    print_cmd_usage(progname, cmd->id);

    return 0;
}

int load_token(uuid_t token)
{
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
        return -1;
    }

    char token_str[37];
    if (errno == EEXIST)
    {
        fd = open(tokfile_path, O_RDONLY);
        if (fd == -1)
        {
            perror("open");
            return -1;
        }
        read(fd, token_str, sizeof(token_str));
        uuid_parse(token_str, token);
        fprintf(stderr, "Read token from file %s\n", tokfile_path);
    }
    else
    {
        uuid_generate(token);
        uuid_unparse(token, token_str);
        write(fd, token_str, sizeof(token_str));
        fprintf(stderr, "Wrote token to file %s\n", tokfile_path);
    }
    return 0;
}

int cmd_auth(char* progname, int argc, char** argv)
{
    (void)argc;
    (void)progname;
    (void)argv;

    Message msg = { 0 };
    msg_init(&msg, MSGTYPE_AUTH_REQ, NULL, 0);

    int res = load_token(msg.hdr.token);
    if (res == -1)
    {
        perror("load_token");
        return 1;
    }

    int sockfd = connect_to_server(server_ip, 1234);
    if (sockfd == -1) return 1;

    res = msg_send(&msg, sockfd, NULL, 0);
    if (res == -1)
    {
        perror("msg_send");
        return 1;
    }

    msg.hdr = (MessageHdr) { 0 };
    msg.payload_len = 0;

    msg_recv(sockfd, &msg, 100);
    fprintf(stderr, "%s: %s\n", msg_type_to_str(msg.hdr.type), msg.payload);

    return 0;
}

int cmd_upload(char* progname, int argc, char** argv)
{
    if (argc < 1)
    {
        print_cmd_usage(progname, CMD_ULOD);
        return 1;
    }

    FileInfo info;
    int ret = fileinfo_from_filename(argv[0], &info);
    if (ret < 0)
    {
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        return 1;
    }

    int sockfd = connect_to_server(server_ip, 1234);
    if (sockfd == -1) return 1;

    printf("filename: %s\n", info.filename);
    printf("size: %zu\n", info.size);
    printf("chunk count: %zu\n", info.chunk_count);
    printf("checksum: ");
    checksum_print(info.checksum);

    Message msg;
    ret = load_token(msg.hdr.token);
    if (ret == -1)
    {
        perror("load_token");
        return 1;
    }

    msg_init(&msg, MSGTYPE_UPLOAD_REQ, (byte*)&info, sizeof(info));
    ret = msg_send(&msg, sockfd, NULL, 0);
    if (ret < 0)
    {
        printf("error when sending UPLOAD_REQ message: ret = %d, errno = %d (%s)\n", ret, errno,
            strerror(errno));
        return 1;
    }

    msg_clear(&msg);
    ret = msg_recv_header(sockfd, &msg);
    if (ret == -1)
    {
        printf("error when receiving UPLOAD_RES or AUTH_FAIL message: ret = %d, errno = %d (%s)\n",
            ret, errno, strerror(errno));
        return 1;
    }
    if (ret == -2)
    {
        printf("peer disconnected gracefully while receiving UPLOAD_RES or AUTH_FAIL: ret = %d\n",
            ret);
        return 1;
    }

    msg_print(&msg);

    if (msg.hdr.type == MSGTYPE_AUTH_FAIL)
    {
        fprintf(stderr, "please authenticate first\n");
        print_cmd_usage(progname, CMD_AUTH);
        return 1;
    }
    else if (msg.hdr.type != MSGTYPE_UPLOAD_RES)
    {
        printf("expected MSGTYPE_UPLOAD_RES, got %s\n", msg_type_to_str(msg.hdr.type));
        return 1;
    }

    struct upload_response_hdr_t {
        bool isError;
        size_t len;
    };
    msg_recv_payload(sockfd, &msg, 4096);
    struct upload_response_hdr_t* upload_res_hdr = (struct upload_response_hdr_t*)msg.payload;

    if (upload_res_hdr->isError)
    {
        printf("error: %s\n", msg.payload + sizeof(struct upload_response_hdr_t));
        return 1;
    }

    ssize_t res = file_send(sockfd, info.filename);
    printf("upload done: res=%zd\n", res);

    msg_clear(&msg);
    msg_recv_header(sockfd, &msg);
    if (msg.hdr.type == MSGTYPE_UPLOAD_FIN)
    {
        printf("upload done!");
    }

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

static int cmd_download(char* progname, int argc, char** argv)
{
    (void)progname;
    (void)argc;
    (void)argv;

    int sockfd = connect_to_server(server_ip, 1234);
    if (sockfd == -1) return 1;

    Message msg;
    msg_init(&msg, MSGTYPE_DOWNLOAD_REQ, NULL, 0);
    msg_send(&msg, sockfd, NULL, 0);
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

static int cmd_list(char* progname, int argc, char** argv)
{
    (void)progname;
    (void)argc;
    (void)argv;

    int sockfd = connect_to_server(server_ip, 1234);
    if (sockfd == -1) return 1;

    Message msg;
    msg_init(&msg, MSGTYPE_LIST_REQ, NULL, 0);
    msg_send(&msg, sockfd, NULL, 0);
    return 0;
}

static int cmd_rm(char* progname, int argc, char** argv)
{
    (void)progname;
    (void)argc;
    (void)argv;

    int sockfd = connect_to_server(server_ip, 1234);
    if (sockfd == -1) return 1;

    Message msg;
    msg_init(&msg, MSGTYPE_REMOVE_REQ, NULL, 0);
    msg_send(&msg, sockfd, NULL, 0);
    return 0;
}
