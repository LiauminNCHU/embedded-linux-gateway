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

#define MAX_MESSAGE_LEN 127
#define BUFFER_SIZE (MAX_MESSAGE_LEN + 2)


message_queue_t queue;
volatile sig_atomic_t running = 1;

/* * ========================= * TCP 客户端线程管理 * ========================= * 
* 每建立一个客户端连接， 
* 就创建一个 client_node
* 
* 所有客户端 node 组成一个链表： 
* 
* client_list 
* ↓ 
* fd=4 → fd=5 → fd=6 
* 
* 主线程退出时遍历这个链表， 
* 对所有 socket 调用 shutdown()， 
* 从而唤醒阻塞在 recv() 的 TCP 线程。 */

typedef struct clinet_node{
    int fd;
    struct client_node *next;
}client_node_t;

/* * 保护 client_list 的 mutex */ 
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER; 

/* * 当前所有客户端 */ 
client_node_t *client_list = NULL; 

/* * 当前还没有退出的 TCP 客户端线程数量 */ 
int client_thread_count = 0; 

/* * 当 TCP 线程退出时， * 用这个条件变量通知主线程。 */ 
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;

void sigint_handler(int sig){
    running = 0;
    /* * write() 是异步信号安全的， * 可以在 signal handler 中使用。 */
    write(STDOUT_FILENO, "SIGINT received\n", 16);
}

int add_client(int fd){
    client_node_t *node = malloc(sizeof(client_node_t));

    if(node == NULL){
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

void remove_client(int fd){
    pthread_mutex_lock(&client_mutex);

    client_node_t **current = &client_list;

    while(*current != NULL){
        if(((*current)->fd == fd)){
            client_node_t *tmp = *current;

            *current = tmp->next;

            free(tmp);

            client_thread_count--;
            /* * 告诉等待中的主线程： * 又有一个 TCP 线程退出了。 */ 
            pthread_cond_broadcast(&client_cond); 
            
            break;
        }

        current = &(*current)->next;
    }

    pthread_mutex_unlock(&client_mutex);
}

/* * ========================= * 关闭所有客户端 socket * ========================= * 
* 注意： * * 这里使用 shutdown() * 而不是 close() * 
* 因为 TCP 线程自己负责 close()。 */ 
void shutdown_all_clients(void) { 
    pthread_mutex_lock(&client_mutex); 
    
    client_node_t *current = client_list; 
    while (current != NULL) { 
        printf("[Main] shutdown client fd=%d\n", current->fd); 
        
        shutdown(current->fd, SHUT_RDWR); 
        
        current = current->next; 
    } 
        
    pthread_mutex_unlock(&client_mutex); 
}


void *tcp_recv_thread(void *arg){
    int client_fd = *(int *)arg;

    free(arg);//直接释放掉malloc的client_fd

    char buffer[BUFFER_SIZE];
    int buffer_len = 0;//有效字节数buffer_len 表示当前 buffer 中已经存放了多少个有效字节，
    //因此下一次 recv() 应该从 buffer[buffer_len] 开始写，也就是传入 buffer + buffer_len

    
    while(running){
        /*
         * buffer 最多保存：
         *
         * 127 字节业务内容
         * + 1 字节 '\n'
         * + 1 字节 '\0'
         *
         * 如果已经达到 128 字节有效数据，
         * 说明 buffer 中已经没有空间继续接收。
         */
        if(buffer_len >= MAX_MESSAGE_LEN + 1){
            fprintf(stderr, "[TCP] message too long");
            break;
        }

        int n = recv(   client_fd,
                        buffer + buffer_len,
                        sizeof(buffer) - 1 - buffer_len,
                        
                        0);
        
        if(n < 0){
            /* * recv() 被信号中断 * * 如果程序正在运行， * 可以继续 recv。 */
            if(errno == EINTR){
                if(!running){
                    break;
                }

                continue;
            }
            perror("recv");
            break;
        }

        if(n == 0){
            printf("[TCP] client fd=%d disconnected\n", client_fd);
            break;
        }

        buffer_len += n;
        buffer[buffer_len] = '\0';
        
        char *p;//找到一个消息结尾的位置\n  p - buffer = \n的下标

        while((p = strchr(buffer, '\n')) != NULL){
            *p = '\0';
            /*
             * 当前协议规定：
             * 单条业务内容最大 127 字节。
             */
            int message_len = p - buffer;

            if (message_len > MAX_MESSAGE_LEN) {
                fprintf(stderr, "[TCP] message too long\n");
                close(client_fd);
                goto thread_exit;
            }

            tcp_message_t message;

            message.id = client_fd;//id 和 客户端对应

            strcpy(message.message, buffer);

            if(queue_push(&queue, &message, &running) != 0){
                goto thread_exit;
            }


            printf("[TCP] message pushed to queue: %s\n", message.message);

            int remaining_len = buffer_len - message_len - 1;
            memmove(buffer, p + 1, remaining_len); // buffer_len - message_len - 1 剩余消息长度

            buffer_len = remaining_len;

            buffer[buffer_len] = '\0';
        }
    }
    

thread_exit: /* * TCP线程退出前： * * 1. 从客户端列表删除自己 * 2. close自己的 socket */ 
    remove_client(client_fd); 
    close(client_fd); 
    printf( "[TCP] thread exited, fd=%d\n", client_fd );

    return NULL;
}

void *work_thread(void *arg){
    tcp_message_t message;

    while(running){
        if(queue_pop(&queue, &message, &running) != 0){
            break;
        }
        
        printf("[Worker] id = %d, message = %s\n", message.id, message.message);

    }

    printf("[Worker] thread exited\n");

    return NULL;
}


int main(void)
{
    /*
        *创建了一个结构体
        结构体清零，
        sa.sa_handler = sigint_handler;如果收到了SIGINT执行函数sigint_handler
        清空信号掩码，处理SIGINT期间不会阻塞其他信号
        sigaction 注册信号处理动作，比signal更加 可靠
    */
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction");
        return 1;
    }


    /*
        创建TCP监听Socket
        AF_INET IPv4
        SOCK_STREAM TCP协议
        0 系统自动选择TCP协议
    */
    //创建Socket 
    int server_fd;

    struct sockaddr_in server_addr;
    //创建Socket        IPv4    TCP字节流，SOCK_DGRAM是UDP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    //地址重用      允许服务器在关闭后立即重新绑定同一个端口，而不需要等待 TIME_WAIT状态结束
    setsockopt( server_fd,   
                SOL_SOCKET,
                SO_REUSEADDR,
                &opt,
                sizeof(opt));


    //本机测试
    /*
        绑定IP和端口

    */


    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind 绑定IP和端口
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    //开始监听
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("TCP server listening on port 8888...\n");


    if(queue_init(&queue) != 0){
        fprintf(stderr, "queue init failed\n");
        close(server_fd);
        return 1;
    }

    pthread_t worker;

    if(pthread_create(&worker, NULL, work_thread, NULL) != 0){
        perror("pthread_create");
        queue_destroy(&queue);
        close(server_fd);
        return 1;
    }

    while(running){
        //接受客户端
        int *client_fd = malloc(sizeof(int));
        //创建失败处理
        if(client_fd == NULL){
            perror("malloc");
            continue;
        }

        *client_fd = accept(server_fd, NULL, NULL);

        if(*client_fd < 0){
            if(!running && errno == EINTR){//Interrupted system call —— 系统调用被信号中断。
                free(client_fd);
                break;
            }

            perror("accept");
            close(server_fd);
            return 1;
        }

        if (!running) { 
            close(*client_fd); 
            free(client_fd); 
            break; 
        }

        printf("connected!fd = %d\n", *client_fd);

        if (add_client(*client_fd) != 0) { 
            fprintf( stderr, "failed to add client\n" ); 
            close(*client_fd); free(client_fd); 
            continue; 
        }

        pthread_t tcp_thread;

        if(pthread_create(&tcp_thread, NULL, tcp_recv_thread, client_fd) != 0){
            perror("pthread create");

            remove_client(*client_fd);

            close(*client_fd);
            
            free(client_fd);
            
            continue;
        }
        
        
        //pthread_join(tcp_thread, NULL);   等待线程结束再继续
        //线程结束后系统自动回收线程资源
        pthread_detach(tcp_thread);

    }
    printf("[Main] shutting down\n");

    //唤醒所有在等待的 notempty notfull
    pthread_cond_broadcast(&queue.not_empty);
    pthread_cond_broadcast(&queue.not_full);
 
    shutdown_all_clients();
    //通过client thread count等待所有TCP线程结束
    pthread_mutex_lock(&client_mutex);
    
    while (client_thread_count > 0) { 
        pthread_cond_wait(&client_cond, &client_mutex);
    } 
    
    pthread_mutex_unlock(&client_mutex);

    //等待指定线程结束 worker
    pthread_join(worker, NULL);

    queue_destroy(&queue);

    close(server_fd);

    pthread_mutex_destroy(&client_mutex);

    pthread_cond_destroy(&client_cond);
    
    printf("[Main] shutting complete\n");

    return 0;
}