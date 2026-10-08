#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8888
#define BUFFER_SIZE 128

int main(void)
{
    int client_fd;

    struct sockaddr_in server_addr;

    char buffer[BUFFER_SIZE];

    client_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (client_fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET,
                  "127.0.0.1",
                  &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(client_fd);
        return 1;
    }

    const char *message1 = "hello";
    const char *message2 = "world";

    int ret = sendto(client_fd,
                     message1,
                     strlen(message1),
                     0,
                     (struct sockaddr *)&server_addr,
                     sizeof(server_addr));

    if (ret < 0) {
        perror("sendto");
        close(client_fd);
        return 1;
    }

    printf("[Client] sent: %s\n", message1);

    ret = sendto(client_fd,
                     message2,
                     strlen(message2),
                     0,
                     (struct sockaddr *)&server_addr,
                     sizeof(server_addr));

    if (ret < 0) {
        perror("sendto");
        close(client_fd);
        return 1;
    }


    printf("[Client] sent: %s\n", message2);

    int n = recvfrom(client_fd,
                     buffer,
                     sizeof(buffer) - 1,
                     0,
                     NULL,
                     NULL);

    if (n < 0) {
        perror("recvfrom");
        close(client_fd);
        return 1;
    }

    buffer[n] = '\0';

    printf("[Client] recv: %s\n", buffer);

    close(client_fd);

    return 0;
}