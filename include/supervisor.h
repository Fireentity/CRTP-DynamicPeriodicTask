#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdatomic.h>
#include <stdbool.h>

#include "event.h"

typedef struct Supervisor Supervisor;

/**
 * Initializes the supervisor queue and synchronization primitives.
 * Must be called before starting the supervisor loop or pushing events.
 */
Supervisor supervisor_init(void);

/**
 * Main loop. Initializes subsystems and processes the event queue.
 */
void supervisor_loop(Supervisor *supervisor);

/**
 * Thread-safe queue insertion.
 */
int supervisor_push_event(Event ev);

#endif
