#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define PORT 9999
#define MAX_EVENTS 10
#define BUFFER_SIZE 128

int main(void)
{
    int server_fd;
    int epfd;

    struct sockaddr_in server_addr;

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    char buffer[BUFFER_SIZE];


    /*
     * =========================
     * 1. 创建 TCP Socket
     * =========================
     */
    server_fd = socket(AF_INET,
                       SOCK_STREAM,
                       0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }


    /*
     * =========================
     * 2. 设置地址
     * =========================
     */
    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr =
        htonl(INADDR_ANY);


    /*
     * =========================
     * 3. bind
     * =========================
     */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("bind");

        close(server_fd);

        return 1;
    }


    /*
     * =========================
     * 4. listen
     * =========================
     */
    if (listen(server_fd, 10) < 0) {

        perror("listen");

        close(server_fd);

        return 1;
    }


    /*
     * =========================
     * 5. 创建 epoll
     * =========================
     */
    epfd = epoll_create1(0);

    if (epfd < 0) {

        perror("epoll_create1");

        close(server_fd);

        return 1;
    }


    /*
     * =========================
     * 6. 将 server_fd 加入 epoll
     * =========================
     *
     * EPOLLIN：
     *
     * fd 有数据可以读取。
     *
     * 对 listen socket 来说，
     * 表示有新的连接可以 accept。
     */
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epfd,
                  EPOLL_CTL_ADD,
                  server_fd,
                  &event) < 0) {

        perror("epoll_ctl");

        close(epfd);
        close(server_fd);

        return 1;
    }


    printf("epoll server listening on port %d...\n",
           PORT);


    /*
     * =========================
     * 7. epoll 主循环
     * =========================
     */
    while (1) {

        int n = epoll_wait(epfd,
                           events,
                           MAX_EVENTS,
                           -1);

        if (n < 0) {

            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");

            break;
        }


        /*
         * 处理所有已经就绪的 fd
         */
        for (int i = 0; i < n; i++) {

            int fd = events[i].data.fd;


            /*
             * =========================
             * 8. listen socket 就绪
             * =========================
             *
             * 表示：
             *
             * 有新的 TCP 客户端连接。
             */
            if (fd == server_fd) {

                int client_fd = accept(
                    server_fd,
                    NULL,
                    NULL
                );

                if (client_fd < 0) {

                    perror("accept");

                    continue;
                }


                printf("[epoll] new client fd=%d\n",
                       client_fd);


                /*
                 * 将新的 client fd
                 * 加入 epoll。
                 */
                event.events = EPOLLIN;
                event.data.fd = client_fd;

                if (epoll_ctl(
                        epfd,
                        EPOLL_CTL_ADD,
                        client_fd,
                        &event) < 0) {

                    perror("epoll_ctl client");

                    close(client_fd);

                    continue;
                }
            }


            /*
             * =========================
             * 9. client socket 就绪
             * =========================
             */
            else {

                int recv_len = recv(
                    fd,
                    buffer,
                    sizeof(buffer) - 1,
                    0
                );


                /*
                 * 客户端关闭连接
                 */
                if (recv_len == 0) {

                    printf(
                        "[epoll] client fd=%d closed\n",
                        fd
                    );

                    epoll_ctl(
                        epfd,
                        EPOLL_CTL_DEL,
                        fd,
                        NULL
                    );

                    close(fd);

                    continue;
                }


                /*
                 * recv 出错
                 */
                if (recv_len < 0) {

                    if (errno == EINTR) {
                        continue;
                    }

                    perror("recv");

                    epoll_ctl(
                        epfd,
                        EPOLL_CTL_DEL,
                        fd,
                        NULL
                    );

                    close(fd);

                    continue;
                }


                /*
                 * 正常收到数据
                 */
                buffer[recv_len] = '\0';

                printf(
                    "[epoll] fd=%d message=%s\n",
                    fd,
                    buffer
                );
            }
        }
    }


    close(epfd);
    close(server_fd);

    return 0;
}