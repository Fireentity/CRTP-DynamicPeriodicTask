#ifndef TASK_ROUTINES_H
#define TASK_ROUTINES_H

#include "task.h"

/**
 * Configuration structure containing the task catalog
 * and CPU calibration parameter.
 */
struct TasksConfig {
    TaskType tasks[N_TASKS];
    unsigned long long loops_per_ms;
};

/**
 * Performs CPU calibration to determine loops_per_ms.
 * Initializes the static task catalog.
 * @return The initialized and calibrated configuration.
 */
void tasks_config_init(TasksConfig* config);

/**
 * Look up a task definition by its name.
 * @param config The config containing the tasks
 * @param name The name string (e.g., "t1").
 * @return Pointer to TaskType or NULL if not found.
 */
const TaskType *tasks_config_get_by_name(const TasksConfig* config,const char *name);


#endif
