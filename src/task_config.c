#include <stdio.h>
#include <string.h>
#include <time.h>
#include "constants.h"
#include "task_config.h"


void tasks_config_init(TasksConfig* config) {
    struct timespec s, e;
    unsigned long long count = 0;
    const long long target_ns = 100000000; // 100 ms

    printf("[Routines] Calibrating CPU (target: 100ms sample)...\n");
    clock_gettime(CLOCK_MONOTONIC, &s);
    do {
        task_run((double) count++);
        clock_gettime(CLOCK_MONOTONIC, &e);
    } while ((double) (e.tv_sec - s.tv_sec) * 1e9 + (double) (e.tv_nsec - s.tv_nsec) < (double) target_ns);

    config->loops_per_ms = count / 100;
    printf("[Routines] Calibration done: %llu loops/ms\n", config->loops_per_ms);
}

const TaskType *tasks_config_get_by_name(const TasksConfig* config,const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < N_TASKS; i++) {
        if (strcmp(config->tasks[i].name, name) == 0) return &config->tasks[i];
    }
    return NULL;
}