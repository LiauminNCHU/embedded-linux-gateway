#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

int pipefd[2];

typedef struct {
    int device_id;
    float temperature;
    float humidity;
} sensor_message_t;


void *sensor_thread(void *arg)
{
    sensor_message_t message;
    message.device_id = 1;
    message.temperature = 20.0;
    message.humidity = 60.0;

    while(1){
        ssize_t n = write(pipefd[1], &message, sizeof(message));   

        if (n != sizeof(message)) {
            perror("write");
            break;
        }

        printf("[sensor] send device_id = %d\n", message.device_id);
        printf("[sensor] send temperature = %f\n", message.temperature);
        printf("[sensor] send humidity = %f\n", message.humidity);
            
        message.temperature+=0.5;
        message.humidity+=0.5;

        sleep(1);
    }

    return NULL;
}

void *monitor_thread(void *arg)
{
    sensor_message_t message;

    while (1) {
        ssize_t n = read(pipefd[0], &message, sizeof(message));

        if (n != sizeof(message)) {
            perror("read");
            break;
        }
        
        printf("[monitor] receive device_id = %d\n", message.device_id);
        printf("[monitor] receive temperature = %f\n", message.temperature);
        printf("[monitor] receive humidity = %f\n", message.humidity);
        
        sleep(3);
    }

    return NULL;
}

int main(void)
{
    pthread_t sensor;
    pthread_t monitor;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    if (pthread_create(&sensor, NULL,
                       sensor_thread, NULL) != 0) {
        perror("pthread_create sensor");
        return 1;
    }

    if (pthread_create(&monitor, NULL,
                       monitor_thread, NULL) != 0) {
        perror("pthread_create monitor");
        return 1;
    }

    pthread_join(sensor, NULL);
    pthread_join(monitor, NULL);

    close(pipefd[0]);
    close(pipefd[1]);


    return 0;
}
