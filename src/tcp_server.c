#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "message_queue.h"
#include "tcp_server.h"


#define TCP_PORT 8888

#define MAX_MESSAGE_LEN 127
#define BUFFER_SIZE (MAX_MESSAGE_LEN + 2)


/*
 * =========================
 * Gateway 全局状态
 * =========================
 *
 * queue 和 running 由 main.c 定义。
 *
 * TCP 模块这里只引用，不负责创建。
 */
extern message_queue_t queue;
extern volatile sig_atomic_t running;


/*
 * =========================
 * TCP 客户端线程管理
 * =========================
 *
 * 每建立一个 TCP 客户端连接，
 * 就创建一个 client_node。
 *
 * 所有客户端组成链表：
 *
 * client_list
 *     ↓
 * fd=4 → fd=5 → fd=6
 *
 * 主线程退出时：
 *
 * shutdown_all_clients()
 *     ↓
 * shutdown(fd, SHUT_RDWR)
 *     ↓
 * 唤醒阻塞在 recv() 的 TCP client thread
 */


/*
 * 一个 TCP 客户端节点
 */
typedef struct client_node {
    int fd;
    struct client_node *next;
} client_node_t;


/*
 * 保护 client_list 的 mutex
 */
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * 当前所有 TCP 客户端
 */
client_node_t *client_list = NULL;


/*
 * 当前还没有退出的 TCP 客户端线程数量
 */
int client_thread_count = 0;


/*
 * TCP client thread 退出时，
 * 通知等待中的线程。
 */
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;


/*
 * =========================
 * 添加 TCP 客户端
 * =========================
 */
int add_client(int fd)
{
    client_node_t *node =
        malloc(sizeof(client_node_t));

    if (node == NULL) {
        return -1;
    }

    node->fd = fd;

    pthread_mutex_lock(&client_mutex);

    node->next = client_list;
    client_list = node;

    client_thread_count++;

    pthread_mutex_unlock(&client_mutex);

    return 0;
}


/*
 * =========================
 * 删除 TCP 客户端
 * =========================
 *
 * TCP client thread 退出时调用。
 */
void remove_client(int fd)
{
    pthread_mutex_lock(&client_mutex);

    client_node_t **current = &client_list;

    while (*current != NULL) {

        if ((*current)->fd == fd) {

            client_node_t *tmp = *current;

            *current = tmp->next;

            free(tmp);

            client_thread_count--;

            /*
             * 通知等待中的线程：
             * 又有一个 TCP client thread 退出。
             */
            pthread_cond_broadcast(&client_cond);

            break;
        }

        current = &(*current)->next;
    }

    pthread_mutex_unlock(&client_mutex);
}


/*
 * =========================
 * 关闭所有 TCP 客户端
 * =========================
 *
 * 注意：
 *
 * 这里使用 shutdown()
 * 而不是 close()。
 *
 * 因为 TCP client thread
 * 自己负责 close()。
 *
 * shutdown()
 *     ↓
 * recv() 返回
 *     ↓
 * TCP client thread 退出
 *     ↓
 * close()
 */
void shutdown_all_clients(void)
{
    pthread_mutex_lock(&client_mutex);

    client_node_t *current = client_list;

    while (current != NULL) {

        printf("[Main] shutdown client fd=%d\n",
               current->fd);

        shutdown(current->fd, SHUT_RDWR);

        current = current->next;
    }

    pthread_mutex_unlock(&client_mutex);
}


/*
 * =========================
 * TCP 客户端线程
 * =========================
 *
 * 每一个 TCP 客户端连接
 * 对应一个线程。
 *
 * 这个线程只负责：
 *
 * recv()
 * TCP 拆包
 * queue_push()
 * close()
 *
 * 不负责：
 *
 * socket()
 * bind()
 * listen()
 * accept()
 */
void *tcp_recv_thread(void *arg)
{
    /*
     * main / tcp_server_thread()
     * 为每个客户端动态分配 fd。
     */
    int client_fd = *(int *)arg;

    /*
     * fd 已经复制到线程自己的栈变量，
     * 所以可以释放外部 malloc 的内存。
     */
    free(arg);


    /*
     * TCP 是字节流，
     * 所以需要自己维护接收缓冲区。
     */
    char buffer[BUFFER_SIZE];

    /*
     * 当前 buffer 中有效数据长度。
     */
    int buffer_len = 0;


    while (running) {

        /*
         * buffer 最多保存：
         *
         * 127 字节业务内容
         * + 1 字节 '\n'
         * + 1 字节 '\0'
         *
         * 如果达到 128 字节有效数据，
         * 说明当前消息可能超过最大长度。
         */
        if (buffer_len >= MAX_MESSAGE_LEN + 1) {

            fprintf(stderr,
                    "[TCP] message too long\n");

            break;
        }


        /*
         * 接收 TCP 字节流。
         */
        int n = recv(client_fd,
                     buffer + buffer_len,
                     sizeof(buffer) - 1 - buffer_len,
                     0);


        /*
         * recv() 出错。
         */
        if (n < 0) {

            /*
             * recv() 被信号中断。
             */
            if (errno == EINTR) {

                /*
                 * 如果程序正在退出，
                 * 直接结束线程。
                 */
                if (!running) {
                    break;
                }

                /*
                 * 正常运行时重新 recv。
                 */
                continue;
            }

            perror("[TCP] recv");

            break;
        }


        /*
         * recv() == 0：
         *
         * 对端正常关闭连接。
         */
        if (n == 0) {

            printf("[TCP] client fd=%d disconnected\n",
                   client_fd);

            break;
        }


        /*
         * 更新当前缓冲区有效长度。
         */
        buffer_len += n;

        /*
         * 添加字符串结束符。
         */
        buffer[buffer_len] = '\0';


        /*
         * =========================
         * TCP 应用层拆包
         * =========================
         *
         * 当前协议：
         *
         * message\n
         *
         * 例如：
         *
         * hello\nworld\n
         *
         * 一次 recv() 可能同时收到：
         *
         * hello\nworld\n
         *
         * 所以这里需要循环处理。
         */
        char *p;

        while ((p = strchr(buffer, '\n')) != NULL) {

            /*
             * 把 '\n' 改成 '\0'，
             * 得到一个 C 字符串。
             */
            *p = '\0';


            /*
             * 当前消息长度。
             *
             * p - buffer
             * 就是 '\n' 的下标。
             */
            int message_len = p - buffer;


            /*
             * 检查单条业务消息长度。
             */
            if (message_len > MAX_MESSAGE_LEN) {

                fprintf(stderr,
                        "[TCP] message too long\n");

                /*
                 * 不在这里 close()。
                 *
                 * 统一交给 thread_exit。
                 */
                goto thread_exit;
            }


            /*
             * =========================
             * 创建统一 Gateway 消息
             * =========================
             */
            gateway_message_t message;

            /*
             * 当前使用客户端 fd
             * 作为消息 ID。
             */
            message.id = client_fd;

            /*
             * 标记消息来源。
             */
            message.source = MESSAGE_SOURCE_TCP;


            /*
             * 将 TCP 数据复制到
             * Gateway 消息中。
             */
            strcpy(message.message,
                   buffer);


            /*
             * =========================
             * 放入统一消息队列
             * =========================
             */
            if (queue_push(&queue,
                           &message,
                           &running) != 0) {

                goto thread_exit;
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
             * buffer:
             *
             * hello\nworld\n
             *      ↑
             *      p
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
                buffer_len - message_len - 1;


            memmove(buffer,
                    p + 1,
                    remaining_len);


            /*
             * 更新 buffer 长度。
             */
            buffer_len = remaining_len;


            /*
             * 重新添加字符串结束符。
             */
            buffer[buffer_len] = '\0';
        }
    }


/*
 * =========================
 * TCP 客户端线程退出
 * =========================
 *
 * 统一处理：
 *
 * 1. 从客户端链表删除自己
 * 2. close socket
 */
thread_exit:

    remove_client(client_fd);

    close(client_fd);

    printf("[TCP] thread exited, fd=%d\n",
           client_fd);

    return NULL;
}


/*
 * =========================
 * TCP Server 线程
 * =========================
 *
 * 负责：
 *
 * socket()
 * bind()
 * listen()
 * accept()
 *
 * 每 accept 一个客户端：
 *
 * accept()
 *     ↓
 * add_client()
 *     ↓
 * pthread_create()
 *     ↓
 * tcp_recv_thread()
 */
void *tcp_server_thread(void *arg)
{
    (void)arg;


    /*
     * =========================
     * 创建 TCP 监听 socket
     * =========================
     */
    int server_fd =
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
     *
     * 允许服务器重新绑定
     * 已经使用过的地址。
     */
    int opt = 1;

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0) {

        perror("[TCP] setsockopt");

        close(server_fd);

        return NULL;
    }


    /*
     * =========================
     * 设置服务器地址
     * =========================
     */
    struct sockaddr_in server_addr;

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

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

        return NULL;
    }


    /*
     * =========================
     * listen
     * =========================
     */
    if (listen(server_fd, 5) < 0) {

        perror("[TCP] listen");

        close(server_fd);

        return NULL;
    }


    printf("[TCP] server listening on port %d...\n",
           TCP_PORT);


    /*
     * =========================
     * accept 客户端
     * =========================
     */
    while (running) {

        /*
         * 动态分配 client fd。
         *
         * 因为这个 fd 要传给新创建的线程。
         */
        int *client_fd =
            malloc(sizeof(int));

        if (client_fd == NULL) {

            perror("[TCP] malloc");

            continue;
        }


        /*
         * 等待客户端连接。
         */
        *client_fd =
            accept(server_fd,
                   NULL,
                   NULL);


        /*
         * accept 出错。
         */
        if (*client_fd < 0) {

            int saved_errno = errno;

            free(client_fd);


            /*
             * Ctrl+C 时：
             *
             * running = 0
             *
             * accept() 可能被信号中断。
             */
            if (!running &&
                saved_errno == EINTR) {

                break;
            }


            /*
             * 如果是其他错误，
             * 继续等待新的连接。
             */
            errno = saved_errno;

            perror("[TCP] accept");

            continue;
        }


        /*
         * accept 成功，
         * 但程序可能已经进入退出流程。
         */
        if (!running) {

            close(*client_fd);

            free(client_fd);

            break;
        }


        printf("[TCP] connected! fd=%d\n",
               *client_fd);


        /*
         * =========================
         * 加入客户端管理链表
         * =========================
         */
        if (add_client(*client_fd) != 0) {

            fprintf(stderr,
                    "[TCP] failed to add client\n");

            close(*client_fd);

            free(client_fd);

            continue;
        }


        /*
         * =========================
         * 创建 TCP 客户端线程
         * =========================
         */
        pthread_t tcp_thread;

        if (pthread_create(&tcp_thread,
                           NULL,
                           tcp_recv_thread,
                           client_fd) != 0) {

            perror("[TCP] pthread_create");


            /*
             * 创建线程失败，
             * 因为这个客户端线程不存在，
             * 所以必须手动从 client_list 删除。
             */
            remove_client(*client_fd);

            close(*client_fd);

            free(client_fd);

            continue;
        }


        /*
         * TCP client thread 不需要
         * 主线程逐个 join。
         *
         * client_list + client_thread_count
         * 负责生命周期管理。
         */
        pthread_detach(tcp_thread);
    }


    /*
     * =========================
     * TCP Server 线程退出
     * =========================
     */
    close(server_fd);

    printf("[TCP] server thread exited\n");

    return NULL;
}


/*
 * =========================
 * 等待 TCP 客户端线程退出
 * =========================
 *
 * main.c 开始关闭 Gateway 后：
 *
 * shutdown_all_clients()
 *          ↓
 * TCP client thread 退出
 *          ↓
 * client_thread_count--
 *          ↓
 * pthread_cond_broadcast()
 *          ↓
 * tcp_server_wait_clients()
 *          ↓
 * 所有 TCP client thread 退出
 *
 * 只有确认所有 TCP client thread
 * 都已经退出后，main.c 才能安全
 * 销毁整个 Gateway 的共享资源。
 */
void tcp_server_wait_clients(void)
{
    pthread_mutex_lock(&client_mutex);

    while (client_thread_count > 0) {
        pthread_cond_wait(&client_cond,
                          &client_mutex);
    }

    pthread_mutex_unlock(&client_mutex);
}


/*
 * =========================
 * TCP 模块资源清理
 * =========================
 *
 * TCP 模块内部负责：
 *
 * client_mutex
 * client_cond
 *
 * main.c 不直接访问这些内部资源。
 */
void tcp_server_cleanup(void)
{
    pthread_mutex_destroy(&client_mutex);
    pthread_cond_destroy(&client_cond);
}