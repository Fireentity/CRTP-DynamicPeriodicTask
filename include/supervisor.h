#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdatomic.h>
#include "event.h"
#include "event_queue.h"
#include "task.h"

typedef struct {
    const TaskType *type;
    int instance_id;
} Task;

typedef struct {
    atomic_bool running;
    EventQueue queue;
    Task active_set[MAX_INSTANCES];
    int active_count;
    pthread_mutex_t active_mutex;
} Supervisor;

/**
 * Initializes the supervisor queue and synchronization primitives.
 * Must be called before starting the supervisor loop or pushing events.
 */
void supervisor_init(Supervisor *supervisor);

/**
 * Main loop. Initializes subsystems and processes the event queue.
 */
void supervisor_loop(Supervisor *supervisor);

/**
 * Thread-safe queue insertion.
 */
int supervisor_push_event(Event ev);

#endif
