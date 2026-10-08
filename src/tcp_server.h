#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <signal.h>
#include <pthread.h>

void *tcp_server_thread(void *arg);

void shutdown_all_clients(void);

void tcp_server_wait_clients(void);

#endif