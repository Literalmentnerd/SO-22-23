#include "producer-consumer.h"
#include <stdlib.h>
#define PIPENAME_SIZE 256
#define BOXNAME_SIZE 32
#define MESSAGELENGTH 289


int g_value = 0;

int pcq_create(pc_queue_t *queue, size_t capacity){
    pthread_mutex_init(&queue->pcq_current_size_lock, NULL);
    pthread_mutex_init(&queue->pcq_head_lock, NULL);
    pthread_mutex_init(&queue->pcq_tail_lock, NULL);
    pthread_mutex_init(&queue->pcq_pusher_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_pusher_condvar, NULL);
    pthread_mutex_init(&queue->pcq_popper_condvar_lock, NULL);
    pthread_cond_init(&queue->pcq_popper_condvar, NULL);
    pthread_mutex_lock(&queue->pcq_head_lock);
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    pthread_mutex_lock(&queue->pcq_tail_lock);
    queue->pcq_capacity = capacity;
    queue->pcq_head = queue->pcq_current_size = 0;
    queue->pcq_tail = capacity - 1;
    queue->pcq_buffer = (void**)malloc(queue->pcq_capacity * sizeof(char*));
    for(int i = 0; i < queue->pcq_capacity; i++) {
        queue->pcq_buffer[i] = (char*)malloc(sizeof(char)*MESSAGELENGTH);
    }
    pthread_mutex_unlock(&queue->pcq_tail_lock);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    pthread_mutex_unlock(&queue->pcq_head_lock);
    return 0;
}

int pcq_destroy(pc_queue_t *queue){
    pthread_mutex_destroy(&queue->pcq_current_size_lock);
    pthread_mutex_destroy(&queue->pcq_head_lock);
    pthread_mutex_destroy(&queue->pcq_tail_lock);
    pthread_mutex_destroy(&queue->pcq_pusher_condvar_lock);
    pthread_cond_destroy(&queue->pcq_pusher_condvar);
    pthread_mutex_destroy(&queue->pcq_popper_condvar_lock);
    pthread_cond_destroy(&queue->pcq_popper_condvar);
    for(int i = 0; i < queue->pcq_capacity; i++) {
        free(queue->pcq_buffer[i]);
    }
    return 0;
}

int pcq_enqueue(pc_queue_t *queue, void *elem){
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    pthread_mutex_lock(&queue->pcq_tail_lock);
    if (queue->pcq_current_size == queue->pcq_capacity){
        pthread_mutex_lock(&queue->pcq_pusher_condvar_lock);
        while(g_value == 0){
            pthread_cond_wait(&queue->pcq_pusher_condvar, &queue->pcq_pusher_condvar_lock);
        }
        pthread_mutex_unlock(&queue->pcq_pusher_condvar_lock);
    } else if(queue->pcq_current_size == 0) {
        g_value--;
        pthread_cond_signal(&queue->pcq_popper_condvar);
    } 
    queue->pcq_tail = (queue->pcq_tail + 1) % queue->pcq_capacity;
    queue->pcq_buffer[queue->pcq_tail] = elem;
    queue->pcq_current_size = queue->pcq_current_size + 1;
    pthread_mutex_unlock(&queue->pcq_tail_lock);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    return 0;
}

void *pcq_dequeue(pc_queue_t *queue){
    pthread_mutex_lock(&queue->pcq_current_size_lock);
    pthread_mutex_lock(&queue->pcq_head_lock);
    if (queue->pcq_current_size == 0){
        pthread_mutex_lock(&queue->pcq_popper_condvar_lock);
        while(g_value == 1){
            pthread_cond_wait(&queue->pcq_popper_condvar, &queue->pcq_popper_condvar_lock);
        }
        pthread_mutex_unlock(&queue->pcq_popper_condvar_lock);
    } else if(queue->pcq_current_size == queue ->pcq_capacity) {
        g_value++;
        pthread_cond_signal(&queue->pcq_pusher_condvar);
    }
    void * elem = queue->pcq_buffer[queue->pcq_head];
    queue->pcq_head = (queue->pcq_head + 1) % queue->pcq_capacity;
    queue->pcq_current_size = queue->pcq_current_size - 1;
    pthread_mutex_unlock(&queue->pcq_head_lock);
    pthread_mutex_unlock(&queue->pcq_current_size_lock);
    return elem;
}