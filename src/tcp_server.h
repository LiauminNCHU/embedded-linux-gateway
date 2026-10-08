#ifndef TCP_SERVER_H
#define TCP_SERVER_H

void *tcp_server_thread(void *arg);

void shutdown_all_clients(void);

void tcp_server_wait_clients(void);

void tcp_server_cleanup(void);

#endif