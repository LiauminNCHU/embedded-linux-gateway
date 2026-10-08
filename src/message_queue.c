#include "message_queue.h"
#include <pthread.h>
#include <stdio.h>
#include <signal.h>



int queue_init(message_queue_t* queue){
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    
    if(pthread_mutex_init(&queue->mutex, NULL) != 0){
        //perror("queue mutex error");
        return -1;
    }
    if (pthread_cond_init(&queue->not_empty, NULL) != 0){
        pthread_mutex_destroy(&queue->mutex);
        //perror("queue not_empty error");
        return -1;
    }
    if (pthread_cond_init(&queue->not_full, NULL) != 0){
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        //perror("queue not_full error");
        return -1;
    }

    return 0;
}


int queue_push(message_queue_t *queue, gateway_message_t *message
                ,volatile sig_atomic_t *running)
{
    pthread_mutex_lock(&queue->mutex);

    while (queue->count == QUEUE_SIZE && *running) {
        // 等待 Queue 有空位
        printf("[queue] full, producer waiting...\n");
        pthread_cond_wait(&queue-> not_full,
                            &queue-> mutex);
    }
    if(!*running){
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    // 把 message 放入 Queue
    queue->buffer[queue->tail] = *message;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    queue->count++;

    // 通知消费者
    pthread_cond_signal(&queue->not_empty);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}


int queue_pop(message_queue_t *queue, gateway_message_t *message
                ,volatile sig_atomic_t *running)
                {
    pthread_mutex_lock(&queue->mutex);

    while(queue->count == 0 && *running){
        printf("[queue] empty, consumer waiting...\n");
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if(!*running){
        pthread_mutex_unlock(&queue->mutex);
        return -1;
    }

    *message = queue->buffer[queue->head];
    queue->count--;
    queue->head = (queue->head + 1) % QUEUE_SIZE;

    pthread_cond_signal(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}


int queue_destroy(message_queue_t *queue){
    //按照创建逆序销毁
    pthread_cond_destroy(&queue->not_full);
    pthread_cond_destroy(&queue->not_empty);
    pthread_mutex_destroy(&queue->mutex);

    return 0;
}