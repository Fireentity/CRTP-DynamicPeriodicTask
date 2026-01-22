#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include "supervisor.h"

#include "event_queue.h"
#include "task_config.h"
#include "task_runtime.h"
#include "tcp_server.h"

void supervisor_init(Supervisor *supervisor) {
    printf("[Supervisor] Subsystem Initialized.\n");

    supervisor->running = ATOMIC_VAR_INIT(true);
    event_queue_init(&supervisor->queue);
    supervisor->active_count = 0;
    pthread_mutex_init(&supervisor->active_mutex, NULL);
}

static int compare_deadline(const void *a, const void *b) {
    const TaskType *ta = *(const TaskType **) a;
    const TaskType *tb = *(const TaskType **) b;
    return (int) (ta->deadline_ms - tb->deadline_ms);
}

static int check_rta(Supervisor *supervisor, const TaskType *candidate) {
    const TaskType *tasks[MAX_INSTANCES + 1];
    int count = 0;

    pthread_mutex_lock(&supervisor->active_mutex);
    for (int i = 0; i < supervisor->active_count; i++) tasks[count++] = supervisor->active_set[i].type;
    pthread_mutex_unlock(&supervisor->active_mutex);
    tasks[count++] = candidate;

    // Utilization Test (Necessary Condition)
    double util = 0;
    for (int i = 0; i < count; i++) {
        util += (double) tasks[i]->wcet_ms / (double) tasks[i]->period_ms;
    }
    if (util > 1.0) {
        printf("[RTA] Rejected %s: Utilization %.2f > 1.0\n", candidate->name, util);
        return 0;
    }

    // Response Time Analysis (Sufficient Condition)
    qsort(tasks, count, sizeof(const TaskType *), compare_deadline);

    for (int i = 0; i < count; i++) {
        double R = (double) tasks[i]->wcet_ms;
        int converged = 0;

        for (int k = 0; k < 100; k++) {
            double I = 0;
            for (int j = 0; j < i; j++) {
                I += ceil(R / (double) tasks[j]->period_ms) * (double) tasks[j]->wcet_ms;
            }
            const double R_new = (double) tasks[i]->wcet_ms + I;

            if (R_new > (double) tasks[i]->deadline_ms) {
                printf("[RTA] Rejected %s: R=%.1f > D=%ld\n", candidate->name, R_new, tasks[i]->deadline_ms);
                return 0;
            }
            if (R_new == R) {
                converged = 1;
                break;
            }
            R = R_new;
        }
        if (!converged) return 0;
    }
    return 1;
}

static void handle_activate(Supervisor *spv, const Event ev) {
    char resp[64];
    const TaskType *task = tasks_config_get_by_name(&tasks_config, ev.payload.task_name);
    Task *active_set = spv->active_set;
    pthread_mutex_t *active_mutex = &spv->active_mutex;
    const int active_count = spv->active_count;

    if (!task) {
        tcp_server_send_response(ev.client_fd, "ERR Unknown Task\n");
        return;
    }

    if (!check_rta(spv, task)) {
        tcp_server_send_response(ev.client_fd, "ERR Schedulability\n");
        return;
    }

    // Pre-check capacity to avoid unnecessary thread spawning
    pthread_mutex_lock(active_mutex);
    if (active_count >= MAX_INSTANCES) {
        pthread_mutex_unlock(active_mutex);
        tcp_server_send_response(ev.client_fd, "ERR System Full\n");
        return;
    }
    pthread_mutex_unlock(active_mutex);

    const int id = runtime_create_instance(task);
    if (id < 0) {
        tcp_server_send_response(ev.client_fd, "ERR System Full\n");
        return;
    }

    pthread_mutex_lock(active_mutex);
    if (active_count < MAX_INSTANCES) {
        active_set[active_count].type = task;
        active_set[active_count].instance_id = id;
        spv->active_count++;
        snprintf(resp, sizeof(resp), "OK ID=%d\n", id);
        printf("[Supervisor] Activated task '%s' as ID %d (Total: %d)\n", task->name, id, active_count);
    } else {
        // Safe fallback in case of race condition
        runtime_stop_instance(id);
        snprintf(resp, sizeof(resp), "ERR System Full\n");
    }
    pthread_mutex_unlock(active_mutex);

    tcp_server_send_response(ev.client_fd, resp);
}

static void handle_deactivate(Supervisor *spv, const Event ev) {
    Task *active_set = spv->active_set;
    pthread_mutex_t *active_mutex = &spv->active_mutex;
    const int active_count = spv->active_count;

    const int id = (int) ev.payload.target_id;

    if (runtime_stop_instance(id) != 0) {
        tcp_server_send_response(ev.client_fd, "ERR Invalid ID\n");
        return;
    }

    pthread_mutex_lock(active_mutex);
    int idx = -1;
    for (int i = 0; i < active_count; i++) {
        if (active_set[i].instance_id == id) {
            idx = i;
            break;
        }
    }
    if (idx != -1) {
        for (int i = idx; i < active_count - 1; i++) active_set[i] = active_set[i + 1];
        spv->active_count--;
    }
    pthread_mutex_unlock(active_mutex);

    tcp_server_send_response(ev.client_fd, "OK\n");
    printf("[Supervisor] Deactivated task ID %d\n", id);
}

static void handle_list(Supervisor *spv, const Event ev) {
    const Task *active_set = spv->active_set;
    pthread_mutex_t *active_mutex = &spv->active_mutex;
    const int active_count = spv->active_count;

    char resp[NET_RESPONSE_BUF_SIZE];
    int off = 0;
    pthread_mutex_lock(active_mutex);
    off += snprintf(resp + off, sizeof(resp) - off, "Running: %d\n", active_count);
    for (int i = 0; i < active_count; i++) {
        if (sizeof(resp) - off < 100) break;
        off += snprintf(resp + off, sizeof(resp) - off, "  [ID %d] %s (C=%ld, T=%ld)\n",
                        active_set[i].instance_id, active_set[i].type->name,
                        active_set[i].type->wcet_ms, active_set[i].type->period_ms);
    }
    pthread_mutex_unlock(active_mutex);
    tcp_server_send_response(ev.client_fd, resp);
}

static void handle_info(const Supervisor *spv, const Event ev) {
    char resp[NET_RESPONSE_BUF_SIZE];
    int off = 0;
    const TaskType *cat = tasks_config.tasks;
    off += snprintf(resp + off, sizeof(resp) - off,
                    "Capacity: %d/%d active\nTasks:\n",
                    spv->active_count,
                    MAX_INSTANCES);
    for (int i = 0; i < N_TASKS; i++) {
        off += snprintf(resp + off, sizeof(resp) - off, "  %s: C=%ld T=%ld D=%ld\n",
                        cat[i].name, cat[i].wcet_ms, cat[i].period_ms, cat[i].deadline_ms);
    }
    tcp_server_send_response(ev.client_fd, resp);
}

void supervisor_loop(Supervisor *supervisor) {
    printf("[Supervisor] Event Loop Started.\n");
    while (1) {
        const Event ev = event_queue_pop(&supervisor->queue);
        switch (ev.type) {
            case EV_ACTIVATE: handle_activate(supervisor, ev);
                break;
            case EV_DEACTIVATE: handle_deactivate(supervisor, ev);
                break;
            case EV_LIST: handle_list(supervisor, ev);
                break;
            case EV_INFO: handle_info(supervisor, ev);
                break;
            case EV_SHUTDOWN:
                printf("[Supervisor] Shutdown signal received.\n");
                return;
            default: break;
        }
    }
}
