#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "message_queue.h"
#include "tcp_server.h"
#include "udp_server.h"


/*
 * =========================
 * Gateway 全局状态
 * =========================
 *
 * queue:
 *     TCP / UDP / MQTT 等网络模块
 *     将消息放入统一消息队列。
 *
 * running:
 *     整个 Gateway 的运行状态。
 *
 *     1 -> 正常运行
 *     0 -> 开始退出
 */
message_queue_t queue;

volatile sig_atomic_t running = 1;


/*
 * =========================
 * SIGINT 信号处理
 * =========================
 *
 * Ctrl + C
 *     ↓
 * SIGINT
 *     ↓
 * running = 0
 *
 * 注意：
 * signal handler 中只做简单操作。
 * 不在这里调用 pthread_mutex_lock、
 * free、printf 等非异步信号安全函数。
 */
void sigint_handler(int sig)
{
    (void)sig;

    running = 0;

    write(STDOUT_FILENO,
          "SIGINT received\n",
          16);
}


/*
 * =========================
 * Worker 线程
 * =========================
 *
 * TCP / UDP
 *      ↓
 * message_queue
 *      ↓
 * Worker
 *
 * Worker 不关心消息来自哪个网络线程，
 * 只负责统一消费消息。
 */
void *work_thread(void *arg)
{
    (void)arg;

    gateway_message_t message;

    while (running) {

        if (queue_pop(&queue,
                      &message,
                      &running) != 0) {
            break;
        }

        const char *source;

        switch (message.source) {

        case MESSAGE_SOURCE_TCP:
            source = "TCP";
            break;

        case MESSAGE_SOURCE_UDP:
            source = "UDP";
            break;

        case MESSAGE_SOURCE_MQTT:
            source = "MQTT";
            break;

        default:
            source = "UNKNOWN";
            break;
        }

        printf("[Worker] source=%s, id=%d, message=%s\n",
               source,
               message.id,
               message.message);
    }

    printf("[Worker] thread exited\n");

    return NULL;
}


int main(void)
{
    /*
     * =========================
     * 1. 注册 SIGINT
     * =========================
     *
     * Ctrl + C
     *     ↓
     * sigint_handler()
     *     ↓
     * running = 0
     */
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigint_handler;

    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT,
                  &sa,
                  NULL) < 0) {

        perror("sigaction");

        return 1;
    }


    /*
     * =========================
     * 2. 初始化消息队列
     * =========================
     */
    if (queue_init(&queue) != 0) {

        fprintf(stderr,
                "queue init failed\n");

        return 1;
    }


    /*
     * =========================
     * 3. 创建 Worker 线程
     * =========================
     */
    pthread_t worker;

    if (pthread_create(&worker,
                       NULL,
                       work_thread,
                       NULL) != 0) {

        perror("pthread_create worker");

        queue_destroy(&queue);

        return 1;
    }


    /*
     * =========================
     * 4. 创建 TCP Server 线程
     * =========================
     *
     * TCP 模块内部负责：
     *
     * socket()
     * bind()
     * listen()
     * accept()
     * TCP client thread
     *
     * main 不再关心 TCP 的具体实现。
     */
    pthread_t tcp_thread;

    if (pthread_create(&tcp_thread,
                       NULL,
                       tcp_server_thread,
                       NULL) != 0) {

        perror("pthread_create tcp");

        running = 0;

        /*
         * 唤醒可能阻塞在 queue
         * 中的 Worker。
         */
        pthread_cond_broadcast(&queue.not_empty);
        pthread_cond_broadcast(&queue.not_full);

        pthread_join(worker, NULL);

        queue_destroy(&queue);

        return 1;
    }


    /*
     * =========================
     * 5. 创建 UDP Server 线程
     * =========================
     *
     * UDP 模块内部负责：
     *
     * socket()
     * bind()
     * recvfrom()
     * queue_push()
     */
    pthread_t udp_thread;

    if (pthread_create(&udp_thread,
                       NULL,
                       udp_server_thread,
                       NULL) != 0) {

        perror("pthread_create udp");

        running = 0;

        /*
         * 唤醒 Worker。
         */
        pthread_cond_broadcast(&queue.not_empty);
        pthread_cond_broadcast(&queue.not_full);

        /*
         * 唤醒 TCP client thread。
         */
        shutdown_all_clients();

        /*
         * 等待 TCP server thread。
         */
        pthread_join(tcp_thread, NULL);

        /*
         * 等待 Worker。
         */
        pthread_join(worker, NULL);

        queue_destroy(&queue);

        return 1;
    }


    /*
     * =========================
     * 6. 主线程等待退出
     * =========================
     *
     * main 不再执行：
     *
     * accept()
     * recv()
     * recvfrom()
     *
     * 这些工作全部交给对应模块。
     *
     * main 只负责整个 Gateway
     * 的生命周期。
     */
    while (running) {
        sleep(1);
    }


    /*
     * =========================
     * 7. 开始关闭 Gateway
     * =========================
     */
    printf("[Main] shutting down\n");


    /*
     * =========================
     * 8. 唤醒消息队列
     * =========================
     *
     * Worker 可能正在：
     *
     * pthread_cond_wait()
     *
     * 这里广播，让它有机会退出。
     */
    pthread_cond_broadcast(&queue.not_empty);
    pthread_cond_broadcast(&queue.not_full);


    /*
     * =========================
     * 9. 关闭所有 TCP 客户端
     * =========================
     *
     * TCP client thread 可能阻塞在：
     *
     * recv()
     *
     * shutdown()
     * 会使 recv() 返回，
     * 从而让 TCP client thread
     * 能够正常退出。
     */
    shutdown_all_clients();


    /*
     * =========================
     * 10. 等待 TCP Server
     * =========================
     *
     * TCP server thread
     * 自己负责：
     *
     * socket
     * bind
     * listen
     * accept
     *
     * 最终退出。
     */
    pthread_join(tcp_thread, NULL);


    /*
     * =========================
     * 11. 等待 UDP Server
     * =========================
     */
    pthread_join(udp_thread, NULL);


    /*
     * =========================
     * 12. 等待 Worker
     * =========================
     */
    pthread_join(worker, NULL);

    /*
    * =========================
    * 13. 等待 TCP Client
    * =========================
    *
    * TCP server thread 退出
    * 不代表已经创建的 TCP
    * client thread 全部退出。
    *
    * 因此这里还需要等待：
    *
    * client_thread_count == 0
    */
    tcp_server_wait_clients();


    /*
     * =========================
     * 14. 销毁消息队列
     * =========================
     *
     * 必须确保所有使用 queue
     * 的线程都已经退出之后，
     * 才能销毁 queue。
     */
    queue_destroy(&queue);


    /*
     * =========================
     * 14. 销毁 TCP 客户端管理资源
     * =========================
     */
    tcp_server_cleanup();

    printf("[Main] shutdown complete\n");

    return 0;
}