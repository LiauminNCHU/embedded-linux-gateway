#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <signal.h>
#include <pthread.h>

#define QUEUE_SIZE 10
#define MAX_MESSAGE_LEN 127
#define BUFFER_SIZE (MAX_MESSAGE_LEN + 2)

// typedef struct{
//     int device_id;
//     float temperature;
//     float humidity;
// }sensor_message_t;

typedef struct{
    int id;
    char message[MAX_MESSAGE_LEN + 1];
}tcp_message_t;

typedef struct {
    tcp_message_t buffer[QUEUE_SIZE];
    
    int head;
    int tail;
    int count;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
}message_queue_t;

int queue_init(message_queue_t* queue);
int queue_push(message_queue_t *queue, tcp_message_t *message, volatile sig_atomic_t *running);
int queue_pop(message_queue_t *queue, tcp_message_t *message, volatile sig_atomic_t *running);
int queue_destroy(message_queue_t *queue);

#endif