#ifndef TASK_H
#define TASK_H

#include <stdbool.h>
#include <pthread.h>
#include "constants.h"

typedef struct {
    const char name[TASK_NAME_LEN];
    const long wcet_ms;
    const long period_ms;
    const long deadline_ms;

    void (* const routine_fn)(void);
} TaskType;

typedef struct {
    int id;
    pthread_t thread;
    const TaskType *type;
    volatile bool stop;
    bool active;
} TaskInstance;

#endif
