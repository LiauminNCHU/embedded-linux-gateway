#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "message_queue.h"
#include "tcp_server.h"


#define TCP_PORT 8888

#define MAX_EVENTS 64

#define MAX_MESSAGE_LEN 127
#define BUFFER_SIZE (MAX_MESSAGE_LEN + 2)


/*
 * =========================
 * Gateway 全局状态
 * =========================
 *
 * queue 和 running 由 main.c 定义。
 */
extern message_queue_t queue;
extern volatile sig_atomic_t running;


/*
 * =========================
 * TCP Client 状态
 * =========================
 *
 * epoll 版本不再：
 *
 * 一个客户端 = 一个线程
 *
 * 而是：
 *
 * 一个客户端 = 一个 client_node
 *
 * client_node 中保存：
 *
 * 1. fd
 * 2. TCP 接收缓冲区
 * 3. 当前缓冲区数据长度
 */
typedef struct tcp_client_node {

    int fd;

    /*
     * TCP 字节流接收缓冲区
     */
    char buffer[BUFFER_SIZE];

    /*
     * 当前 buffer 中有效数据长度
     */
    int buffer_len;

    /*
     * 下一个客户端
     */
    struct tcp_client_node *next;

} tcp_client_node_t;


/*
 * =========================
 * TCP Client 管理
 * =========================
 */
static pthread_mutex_t client_mutex =
    PTHREAD_MUTEX_INITIALIZER;

static tcp_client_node_t *client_list = NULL;


/*
 * =========================
 * TCP Server / epoll 状态
 * =========================
 */
static int epoll_fd = -1;

static int server_fd = -1;


/*
 * =========================
 * 设置非阻塞
 * =========================
 */
static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -1;
    }

    if (fcntl(fd,
              F_SETFL,
              flags | O_NONBLOCK) == -1) {
        return -1;
    }

    return 0;
}


/*
 * =========================
 * 添加客户端
 * =========================
 */
static int add_client(tcp_client_node_t *client)
{
    pthread_mutex_lock(&client_mutex);

    client->next = client_list;

    client_list = client;

    pthread_mutex_unlock(&client_mutex);

    return 0;
}


/*
 * =========================
 * 删除客户端
 * =========================
 */
static void remove_client(tcp_client_node_t *client)
{
    pthread_mutex_lock(&client_mutex);

    tcp_client_node_t **current =
        &client_list;

    while (*current != NULL) {

        if (*current == client) {

            *current = client->next;

            break;
        }

        current = &(*current)->next;
    }

    pthread_mutex_unlock(&client_mutex);
}


/*
 * =========================
 * 关闭一个客户端
 * =========================
 */
static void close_client(tcp_client_node_t *client)
{
    if (client == NULL) {
        return;
    }

    /*
     * 从 epoll 中删除
     */
    if (epoll_fd >= 0) {

        epoll_ctl(epoll_fd,
                  EPOLL_CTL_DEL,
                  client->fd,
                  NULL);
    }


    printf("[TCP] client fd=%d disconnected\n",
           client->fd);


    /*
     * 从客户端链表删除
     */
    remove_client(client);


    /*
     * 关闭 socket
     */
    close(client->fd);


    /*
     * 释放 client 节点
     */
    free(client);
}


/*
 * =========================
 * 关闭所有 TCP Client
 * =========================
 *
 * main.c 收到 SIGINT 后调用。
 *
 * epoll 版本没有 client thread，
 * 所以这里不再通过 shutdown()
 * 去唤醒 recv()。
 *
 * 这里直接关闭所有客户端。
 */
void shutdown_all_clients(void)
{
    pthread_mutex_lock(&client_mutex);

    tcp_client_node_t *current =
        client_list;

    while (current != NULL) {

        printf("[Main] shutdown client fd=%d\n",
               current->fd);

        shutdown(current->fd,
                 SHUT_RDWR);

        current = current->next;
    }

    pthread_mutex_unlock(&client_mutex);
}


/*
 * =========================
 * 处理完整 TCP 消息
 * =========================
 *
 * 当前协议：
 *
 *     message\n
 *
 * 例如：
 *
 *     hello\nworld\n
 *
 * 一次 recv() 可能收到：
 *
 *     hello\nworld\n
 *
 * 也可能收到：
 *
 *     hel
 *
 * 下一次：
 *
 *     lo\n
 *
 * 所以每个 client 都必须保存
 * 自己独立的 buffer。
 */
static int process_client_messages(
    tcp_client_node_t *client)
{
    char *p;

    while ((p = strchr(client->buffer, '\n')) != NULL) {

        /*
         * 找到一条完整消息。
         */
        *p = '\0';


        /*
         * 计算消息长度。
         */
        int message_len =
            p - client->buffer;


        /*
         * 检查消息长度。
         */
        if (message_len > MAX_MESSAGE_LEN) {

            fprintf(stderr,
                    "[TCP] message too long, fd=%d\n",
                    client->fd);

            return -1;
        }


        /*
         * =========================
         * 创建统一 Gateway 消息
         * =========================
         */
        gateway_message_t message;

        message.id = client->fd;

        message.source =
            MESSAGE_SOURCE_TCP;


        strcpy(message.message,
               client->buffer);


        /*
         * =========================
         * 放入消息队列
         * =========================
         */
        if (queue_push(&queue,
                       &message,
                       &running) != 0) {

            return -1;
        }


        printf("[TCP] message pushed to queue: %s\n",
               message.message);


        /*
         * =========================
         * 处理剩余数据
         * =========================
         *
         * 例如：
         *
         * hello\nworld\n
         *
         * 当前处理：
         *
         * hello
         *
         * 剩余：
         *
         * world\n
         */
        int remaining_len =
            client->buffer_len -
            message_len -
            1;


        memmove(client->buffer,
                p + 1,
                remaining_len);


        client->buffer_len =
            remaining_len;


        client->buffer[
            client->buffer_len
        ] = '\0';
    }

    return 0;
}


/*
 * =========================
 * 读取 TCP Client 数据
 * =========================
 */
static int handle_client_read(
    tcp_client_node_t *client)
{
    while (1) {

        /*
         * buffer 已经没有空间
         *
         * 说明当前消息没有出现 '\n'，
         * 并且已经超过最大长度。
         */
        if (client->buffer_len >=
            MAX_MESSAGE_LEN + 1) {

            fprintf(stderr,
                    "[TCP] message too long, fd=%d\n",
                    client->fd);

            return -1;
        }


        /*
         * 计算剩余空间。
         *
         * 留一个字节给 '\0'。
         */
        int available =
            sizeof(client->buffer) -
            1 -
            client->buffer_len;


        /*
         * 非阻塞 recv()
         */
        int n =
            recv(client->fd,
                 client->buffer +
                     client->buffer_len,
                 available,
                 0);


        /*
         * recv 出错
         */
        if (n < 0) {

            /*
             * 非阻塞 socket：
             *
             * 当前已经没有更多数据。
             */
            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {

                break;
            }


            /*
             * 被信号中断。
             */
            if (errno == EINTR) {
                continue;
            }


            perror("[TCP] recv");

            return -1;
        }


        /*
         * 对端正常关闭。
         */
        if (n == 0) {

            return -1;
        }


        /*
         * 更新 buffer 长度。
         */
        client->buffer_len += n;


        /*
         * 添加字符串结束符。
         */
        client->buffer[
            client->buffer_len
        ] = '\0';


        /*
         * 尝试处理完整消息。
         */
        if (process_client_messages(client) != 0) {

            return -1;
        }


        /*
         * LT 模式下继续读取，
         * 直到 recv() 返回 EAGAIN。
         */
    }

    return 0;
}


/*
 * =========================
 * accept 新客户端
 * =========================
 */
static int accept_clients(void)
{
    while (1) {

        /*
         * 非阻塞 accept
         */
        int client_fd =
            accept(server_fd,
                    NULL,
                    NULL
                    );


        /*
         * 没有更多连接
         */
        if (client_fd < 0) {

            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {

                return 0;
            }

            if (errno == EINTR) {
                continue;
            }

            perror("[TCP] accept");

            return -1;
        }

        /*
        * 设置客户端 socket 为非阻塞。
        */
        if (set_nonblocking(client_fd) < 0) {

            perror("[TCP] set_nonblocking");

            close(client_fd);

            continue;
        }


        /*
         * 分配 client 节点
         */
        tcp_client_node_t *client =
            calloc(1,
                   sizeof(tcp_client_node_t));

        if (client == NULL) {

            perror("[TCP] calloc");

            close(client_fd);

            continue;
        }


        client->fd = client_fd;

        client->buffer_len = 0;

        client->next = NULL;


        /*
         * 加入客户端管理链表
         */
        add_client(client);


        /*
         * =========================
         * 加入 epoll
         * =========================
         */
        struct epoll_event event;

        memset(&event,
               0,
               sizeof(event));


        /*
         * EPOLLIN：
         * 有数据可读
         *
         * EPOLLRDHUP：
         * 对端关闭连接
         */
        event.events =
            EPOLLIN |
            EPOLLRDHUP;


        /*
         * 把 client 指针保存到 epoll
         */
        event.data.ptr = client;


        if (epoll_ctl(epoll_fd,
                      EPOLL_CTL_ADD,
                      client_fd,
                      &event) < 0) {

            perror("[TCP] epoll_ctl ADD");

            remove_client(client);

            close(client_fd);

            free(client);

            continue;
        }


        printf("[TCP] connected! fd=%d\n",
               client_fd);
    }
}


/*
 * =========================
 * TCP Server Thread
 * =========================
 *
 * 负责：
 *
 * socket
 * bind
 * listen
 * epoll
 * accept
 * recv
 * TCP 拆包
 * queue_push
 */
void *tcp_server_thread(void *arg)
{
    (void)arg;


    /*
     * =========================
     * socket
     * =========================
     */
    server_fd =
        socket(AF_INET,
               SOCK_STREAM,
               0);

    if (server_fd < 0) {

        perror("[TCP] socket");

        return NULL;
    }


    /*
     * =========================
     * SO_REUSEADDR
     * =========================
     */
    int opt = 1;

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0) {

        perror("[TCP] setsockopt");

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    /*
     * =========================
     * 设置地址
     * =========================
     */
    struct sockaddr_in server_addr;

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family =
        AF_INET;

    server_addr.sin_port =
        htons(TCP_PORT);

    server_addr.sin_addr.s_addr =
        htonl(INADDR_ANY);


    /*
     * =========================
     * bind
     * =========================
     */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("[TCP] bind");

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    /*
     * =========================
     * listen
     * =========================
     */
    if (listen(server_fd, 64) < 0) {

        perror("[TCP] listen");

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    /*
     * =========================
     * 设置非阻塞
     * =========================
     */
    if (set_nonblocking(server_fd) < 0) {

        perror("[TCP] set_nonblocking");

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    printf("[TCP] server listening on port %d...\n",
           TCP_PORT);


    /*
     * =========================
     * 创建 epoll
     * =========================
     */
    epoll_fd =
        epoll_create1(0);

    if (epoll_fd < 0) {

        perror("[TCP] epoll_create1");

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    /*
     * =========================
     * 把 listen fd 加入 epoll
     * =========================
     */
    struct epoll_event event;

    memset(&event,
           0,
           sizeof(event));

    event.events =
        EPOLLIN;

    event.data.fd =
        server_fd;


    if (epoll_ctl(epoll_fd,
                  EPOLL_CTL_ADD,
                  server_fd,
                  &event) < 0) {

        perror("[TCP] epoll_ctl ADD server");

        close(epoll_fd);

        epoll_fd = -1;

        close(server_fd);

        server_fd = -1;

        return NULL;
    }


    /*
     * =========================
     * epoll Event Loop
     * =========================
     */
    struct epoll_event events[MAX_EVENTS];


    while (running) {

        /*
         * timeout = 1000ms
         *
         * 每秒检查一次 running。
         */
        int event_count =
            epoll_wait(epoll_fd,
                       events,
                       MAX_EVENTS,
                       1000);


        /*
         * epoll_wait 出错
         */
        if (event_count < 0) {

            if (errno == EINTR) {
                continue;
            }

            perror("[TCP] epoll_wait");

            break;
        }


        /*
         * timeout
         */
        if (event_count == 0) {
            continue;
        }


        /*
         * =========================
         * 处理所有就绪事件
         * =========================
         */
        for (int i = 0;
             i < event_count;
             i++) {

            /*
             * =====================
             * listen fd 有事件
             * =====================
             */
            if (events[i].data.fd ==
                server_fd) {

                if (accept_clients() != 0) {
                    /*
                     * accept 出现严重错误，
                     * 暂时继续运行。
                     */
                }

                continue;
            }


            /*
             * =====================
             * Client fd 有事件
             * =====================
             */
            tcp_client_node_t *client =
                events[i].data.ptr;


            /*
             * Client 连接已经断开，
             * 或出现错误。
             */
            if (events[i].events &
                (EPOLLRDHUP |
                 EPOLLHUP |
                 EPOLLERR)) {

                close_client(client);

                continue;
            }


            /*
             * =====================
             * Client 有数据可读
             * =====================
             */
            if (events[i].events &
                EPOLLIN) {

                if (handle_client_read(client) != 0) {

                    close_client(client);

                    continue;
                }
            }
        }
    }


    /*
     * =========================
     * TCP Server 退出
     * =========================
     *
     * 关闭所有 client。
     */
    pthread_mutex_lock(&client_mutex);

    tcp_client_node_t *current =
        client_list;

    while (current != NULL) {

        tcp_client_node_t *next =
            current->next;

        close(current->fd);

        free(current);

        current = next;
    }

    client_list = NULL;

    pthread_mutex_unlock(&client_mutex);


    /*
     * 从 epoll 中删除 server fd
     */
    if (epoll_fd >= 0) {

        epoll_ctl(epoll_fd,
                  EPOLL_CTL_DEL,
                  server_fd,
                  NULL);

        close(epoll_fd);

        epoll_fd = -1;
    }


    /*
     * 关闭 listen socket
     */
    if (server_fd >= 0) {

        close(server_fd);

        server_fd = -1;
    }


    printf("[TCP] server thread exited\n");

    return NULL;
}


/*
 * =========================
 * TCP 模块资源清理
 * =========================
 *
 * client_mutex 是 TCP 模块内部资源。
 *
 * main.c 不应该直接操作它。
 */
void tcp_server_cleanup(void)
{
    pthread_mutex_destroy(&client_mutex);
}