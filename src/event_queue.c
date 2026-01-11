#include <pthread.h>

#include "event_queue.h"

int event_queue_push(EventQueue* queue, const Event ev) {
    int has_pushed = -1;
    pthread_mutex_lock(&queue->mutex);
    if (queue->count < MAX_QUEUE_SIZE) {
        queue->buffer[queue->tail] = ev;
        queue->tail = (queue->tail + 1) % MAX_QUEUE_SIZE;
        queue->count++;
        pthread_cond_signal(&queue->cond);
        has_pushed = 0;
    }
    pthread_mutex_unlock(&queue->mutex);
    return has_pushed;
}

Event event_queue_pop(EventQueue* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->count == 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    const Event ev = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % MAX_QUEUE_SIZE;
    queue->count--;
    pthread_mutex_unlock(&queue->mutex);
    return ev;
}

EventQueue event_queue_init() {
    const EventQueue queue = {
        .head = 0,
        .tail = 0,
        .count = 0,
        .mutex = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER
    };
    return queue;
}