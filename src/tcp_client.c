#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 8888

int main(void)
{
    int fd;
    struct sockaddr_in server_addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, "127.0.0.1",
                  &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return 1;
    }

    if (connect(fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    send(fd, "hello\n", 6, 0);
    send(fd, "world\n", 6, 0);


    /*
     * 测试 1：
     * 127 个业务字符 + '\n'
     */
    // char message_127[129];

    // memset(message_127, 'a', 127);

    // message_127[127] = '\n';
    // message_127[128] = '\0';

    // printf("send 127 chars + newline\n");

    // send(fd, message_127, 128, 0);

    // sleep(1);

    // /*
    //  * 测试 2：
    //  * 128 个业务字符 + '\n'
    //  */
    // char message_128[130];

    // memset(message_128, 'a', 128);

    // message_128[128] = '\n';
    // message_128[129] = '\0';

    // printf("send 128 chars + newline\n");

    // send(fd, message_128, 129, 0);

    // sleep(1);

    close(fd);

    return 0;
}