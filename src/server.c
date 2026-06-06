#include <stdio.h>

#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

#include "msg.h"

#define PORT 1234

int main(void)
{
    /*
     * Create socket
     */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return 1;
    }

    /*
     * Configure address
     */
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    /*
     * Bind socket to address
     */
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    /*
     * Listen for inbound connections
     */
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }
    printf("Listening at 0.0.0.0:%d\n\n", PORT);

    /*
     * Accept new connection
     */
    int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
    if (new_socket < 0)
    {
        perror("accept");
        close(server_fd);
        return 1;
    }

    Message msg;
    msg_recv(new_socket, &msg);

    char token[37];
    uuid_unparse(msg.data, token);

    printf("Received message: \n");
    printf("    type: %s\n", msg_type_to_str(msg.hdr.type));
    printf("    length: %zu\n", msg.hdr.length);
    printf("    sent_at: %zus, %zuns\n", msg.hdr.sent_at.tv_sec, msg.hdr.sent_at.tv_nsec);
    printf("    data: %s\n", token);
    return 0;
}
