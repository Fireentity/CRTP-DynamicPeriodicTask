#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H
#include "event.h"

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int head;
    int tail;
    int count;
    Event buffer[MAX_QUEUE_SIZE];
} EventQueue;

int event_queue_push(EventQueue* queue, Event ev);

Event event_queue_pop(EventQueue* queue);

void event_queue_init(EventQueue *queue);

#endif //EVENT_QUEUE_H