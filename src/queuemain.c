#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "message_queue.h"

message_queue_t queue;

volatile sig_atomic_t running = 1;

void sigint_handler(int sig){
    running = 0;
}

void *sensor_thread(void *arg)
{
    sensor_message_t message;

    message.device_id = 1;
    message.temperature = 20.0;
    message.humidity = 60.0;

    while (running) {
        //如果退出了直接跳出循环
        if(queue_push(&queue, &message, &running) != 0)
            break;

        printf("[sensor] send device_id = %d\n",
               message.device_id);
        printf("[sensor] send temperature = %f\n",
               message.temperature);
        printf("[sensor] send humidity = %f\n",
               message.humidity);

        message.temperature += 0.5;
        message.humidity += 0.5;

        usleep(100000);
    }

    return NULL;
}

void *monitor_thread(void *arg)
{
    sensor_message_t message;

    while (running) {

        if(queue_pop(&queue, &message, &running) != 0)
            break;

        printf("[monitor] receive device_id = %d\n",
               message.device_id);
        printf("[monitor] receive temperature = %f\n",
               message.temperature);
        printf("[monitor] receive humidity = %f\n",
               message.humidity);

        sleep(3);
    }

    return NULL;
}

int main(void){
    signal(SIGINT, sigint_handler);

    pthread_t sensor;
    pthread_t monitor;

    if(queue_init(&queue) != 0){
        printf("queue init failed\n");
        return 1;
    }

    pthread_create(&sensor, NULL, sensor_thread, NULL);
    pthread_create(&monitor, NULL, monitor_thread, NULL);
    
    while(running){
        sleep(1);
    }

    
    pthread_cond_broadcast(&queue.not_empty);
    pthread_cond_broadcast(&queue.not_full);
    
    pthread_join(sensor, NULL);
    pthread_join(monitor, NULL);

    queue_destroy(&queue);

    return 0;
}