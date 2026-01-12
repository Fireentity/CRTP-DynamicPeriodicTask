#ifndef CONSTANTS_H
#define CONSTANTS_H

#define SERVER_PORT 8080
#define CPU_NUMBER 0

#define MAX_CLIENTS 25
#define BACKLOG_SIZE 5
#define NET_BUFFER_SIZE 4096
#define NET_RESPONSE_BUF_SIZE 4096

#define N_TASKS 3
#define MAX_INSTANCES 20
#define MAX_QUEUE_SIZE 20
#define TASK_NAME_LEN 32
#include <poll.h>
#include "task_config.h"
#include "task.h"

static void task_A(void);
static void task_B(void);
static void task_C(void);

static TasksConfig tasks_config = {
    .tasks = {
        {"t1", 50, 300, 300, task_A},
        {"t2", 100, 500, 500, task_B},
        {"t3", 200, 1000, 1000, task_C}
    }
};

static void task_A(void) { task_run_for(&tasks_config, 50); }
static void task_B(void) { task_run_for(&tasks_config, 100); }
static void task_C(void) { task_run_for(&tasks_config, 200); }


#endif
