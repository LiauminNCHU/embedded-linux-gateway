#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

void *tcp_recv_thread(void *arg){
    int client_fd = *(int *)arg;
    char buffer[128];

    while(1){
        int n = recv(   client_fd,
                        buffer,
                        sizeof(buffer) - 1,
                        0);

        if(n < 0){
            perror("recv");
            break;
        }
        if(n == 0){
            printf("client disconnected\n");
            break;
        }

        buffer[n] = '\0';

        printf("[TCP] recv: %s\n", buffer);
    }

    return NULL;
}

int main(void)
{
    //创建Socket 
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    //创建Socket        IPv4    TCP字节流，SOCK_DGRAM是UDP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;

    setsockopt( server_fd,   
                SOL_SOCKET,
                SO_REUSEADDR,
                &opt,
                sizeof(opt));


    //本机测试
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
    
    //接受客户端
    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        return 1;
    }

    // char buffer[128];

    //          客户端socket  收到的数据     只接受127字节
    //                                最后的\0自己补充
    //int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    // while(1){
    //     int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    //     if(n < 0){
    //         perror("recv");
    //         break;
    //     }

    //     if(n == 0){
    //         printf("client disconnected\n");
    //         break;
    //     }

    //     buffer[n] = '\0';

    //     printf("recv:%s\n", buffer);

    // }

    // if(n < 0){
    //     perror("recv");

    //     close(client_fd);
    //     close(server_fd);
    //     return 1;
    // }

    // buffer[n] = '\0';
    // printf("recv:%s\n", buffer);

    // const char *reply = "hello from server";

    // int ret = send( client_fd,  //发送给哪个客户端
    //                 reply,      //发送的数据
    //                 strlen(reply),  //发送多少字节
    //                 0);         //默认行为
    
    // if(ret < 0){
    //     perror("send");
    // }

    printf("client connected!\n");

    pthread_t tcp_thread;

    pthread_create( &tcp_thread,
                    NULL,
                    tcp_recv_thread,
                    &client_fd);

    pthread_join(tcp_thread, NULL);

    printf("main thread continues!\n");

    close(client_fd);
    close(server_fd);

    return 0;
}