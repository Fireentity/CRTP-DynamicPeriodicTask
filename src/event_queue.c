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

void event_queue_init(EventQueue *queue) {
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
}
