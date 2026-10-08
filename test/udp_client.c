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

    /*
     * =========================
     * 1. 创建 UDP Socket
     * =========================
     *
     * SOCK_DGRAM 表示 UDP。
     */
    client_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (client_fd < 0) {
        perror("socket");
        return 1;
    }


    /*
     * =========================
     * 2. 配置服务器地址
     * =========================
     *
     * 127.0.0.1:8888
     */
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


    /*
     * =========================
     * 3. 交互式发送
     * =========================
     *
     * 用户输入一行，
     * 就发送一个 UDP 数据报。
     */
    printf("Enter message (Ctrl+D to exit):\n");

    while (fgets(buffer,
                 sizeof(buffer),
                 stdin) != NULL) {

        /*
         * 删除 fgets() 保存的换行符。
         *
         * 例如：
         *
         * "hello\n"
         *
         * 变成：
         *
         * "hello"
         */
        buffer[strcspn(buffer, "\n")] = '\0';

        /*
         * 空消息不发送。
         */
        if (buffer[0] == '\0') {
            continue;
        }


        /*
         * =========================
         * 4. sendto()
         * =========================
         *
         * 每调用一次 sendto()
         * 就发送一个 UDP 数据报。
         */
        int ret = sendto(client_fd,
                         buffer,
                         strlen(buffer),
                         0,
                         (struct sockaddr *)&server_addr,
                         sizeof(server_addr));

        if (ret < 0) {

            perror("sendto");

            break;
        }


        printf("[Client] sent: %s\n", buffer);
    }


    /*
     * =========================
     * 5. 关闭 Socket
     * =========================
     */
    close(client_fd);

    printf("[Client] exited\n");

    return 0;
}