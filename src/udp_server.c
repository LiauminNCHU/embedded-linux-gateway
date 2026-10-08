#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

#include "message_queue.h"

#define UDP_PORT 8888
#define UDP_BUFFER_SIZE 128

extern message_queue_t queue;
extern volatile sig_atomic_t running;

void *udp_server_thread(void *arg)
{    
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    char buffer[UDP_BUFFER_SIZE];

    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (udp_fd < 0) {
        perror("[UDP] socket");
        return NULL;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("[UDP] bind");
        close(udp_fd);
        return NULL;
    }

    printf("[UDP] server listening on port %d...\n", UDP_PORT);

    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(udp_fd,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &timeout,
                sizeof(timeout)) < 0) {
        perror("[UDP] setsockopt SO_RCVTIMEO");
        close(udp_fd);
        udp_fd = -1;
        return NULL;
    }

    while (running) {

        socklen_t client_addr_len = sizeof(client_addr);

        int n = recvfrom(udp_fd,
                         buffer,
                         sizeof(buffer) - 1,
                         0,
                         (struct sockaddr *)&client_addr,
                         &client_addr_len);

        if (n < 0) {
            //“资源暂时不可用” 或 “操作会被阻塞”
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            //系统调用被信号中断（Interrupted system call）
            if (errno == EINTR) {
                continue;
            }

            perror("[UDP] recvfrom");
            break;
        }

        if(!running){
            break;
        }

        buffer[n] = '\0';

        gateway_message_t message;

        /*
         * UDP 没有 TCP 那样的 client fd。
         * 当前阶段先使用 0 表示 UDP 消息。
         */
        message.id = 0;

        message.source = MESSAGE_SOURCE_UDP;

        memcpy(message.message, buffer, n + 1);

        if (queue_push(&queue, &message, &running) != 0) {
            break;
        }

        char client_ip[INET_ADDRSTRLEN];

        inet_ntop(AF_INET,
                  &client_addr.sin_addr,
                  client_ip,
                  sizeof(client_ip));

        printf("[UDP] message pushed: %s from %s:%d\n",
               message.message,
               client_ip,
               ntohs(client_addr.sin_port));
    }

    close(udp_fd);

    printf("[UDP] thread exited\n");

    return NULL;
}



// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/socket.h>

// #define PORT 8888
// #define BUFFER_SIZE 128

// int main(){
//     int server_fd;

//     struct sockaddr_in server_addr;
//     struct sockaddr_in client_addr;

//     char buffer[BUFFER_SIZE];

//     socklen_t client_addr_len = sizeof(client_addr);

//     server_fd = socket(AF_INET, SOCK_DGRAM, 0);

//     if(server_fd < 0){
//         perror("socket");
//         return 1;
//     }

//     memset(&server_addr, 0, sizeof(server_addr));

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

//     if(bind(server_fd,
//              (struct sockaddr *)&server_addr,
//              sizeof(server_addr)) < 0) {
//         perror("bind");
//         close(server_fd);
//         return 1;
//     }

//     printf("UDP server listening on port %d...\n", PORT);

//     char client_ip[INET_ADDRSTRLEN];

//     while (1) {
//         client_addr_len = sizeof(client_addr);

//         int n = recvfrom(server_fd,
//                          buffer,
//                          sizeof(buffer) - 1,
//                          0,
//                          (struct sockaddr *)&client_addr,
//                          &client_addr_len);

//         if (n < 0) {
//             perror("recvfrom");
//             break;
//         }

//         buffer[n] = '\0';

//         printf("[UDP] recv: %s\n", buffer);

//         int ret = sendto(server_fd,
//                          buffer,
//                          n,
//                          0,
//                          (struct sockaddr *)&client_addr,
//                          client_addr_len);

//         if (ret < 0) {
//             perror("sendto");
//             break;
//         }


//         inet_ntop(AF_INET,
//                 &client_addr.sin_addr,
//                 client_ip,
//                 sizeof(client_ip));

//         printf("[UDP] from %s:%d\n",
//             client_ip,
//             ntohs(client_addr.sin_port));
//     }

//     close(server_fd);

//     return 0;
// }