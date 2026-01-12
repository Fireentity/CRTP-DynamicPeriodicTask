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

// forward declaration
typedef struct TasksConfig TasksConfig;

/**
 * Executes a single unit of dummy computational work.
 * Performs mathematical operations (sqrt, sin) to burn CPU cycles without sleeping.
 * @param i Input value for the calculation (prevents compiler optimization).
 */
void task_run(double i);

/**
 * Executes the dummy workload for a specific duration.
 * Relies on the calibrated `loops_per_ms` to determine how many iterations to run.
 * @param ms The target execution time in milliseconds (WCET).
 */
void task_run_for(const TasksConfig* config, long ms);
#endif
