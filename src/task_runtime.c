#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>
#include <errno.h>
#include <signal.h>
#include "constants.h"
#include "task_runtime.h"

static TaskInstance pool[MAX_INSTANCES];
static pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int id_counter = 1;

#define NSEC_PER_SEC 1000000000L
#define MSEC_PER_NSEC 1000000LL

/*
 * Returns a new timespec representing 'ts' + 'ns'.
 * The original 'ts' remains unmodified.
 */
struct timespec timespec_add_ns(struct timespec ts, long ns) {
    // 1. Add the seconds part of the nanoseconds
    ts.tv_sec += ns / NSEC_PER_SEC;

    // 2. Add the remaining nanoseconds
    ts.tv_nsec += ns % NSEC_PER_SEC;

    // 3. Handle the carry over if tv_nsec became >= 1 billion
    if (ts.tv_nsec >= NSEC_PER_SEC) {
        ts.tv_nsec -= NSEC_PER_SEC;
        ts.tv_sec++;
    }

    return ts;
}

static inline int timespec_cmp(const struct timespec *a, const struct timespec *b) {
    if (a->tv_sec != b->tv_sec) return (a->tv_sec > b->tv_sec) ? 1 : -1;
    if (a->tv_nsec != b->tv_nsec) return (a->tv_nsec > b->tv_nsec) ? 1 : -1;
    return 0;
}

static long long diff_ns(const struct timespec t1, const struct timespec t2) {
    return (long long) (t2.tv_sec - t1.tv_sec) * NSEC_PER_SEC + (t2.tv_nsec - t1.tv_nsec);
}

static void *thread_entry(void *arg) {
    const TaskInstance *inst = (TaskInstance *) arg;
    struct timespec current_activation, start, end;
    const long long deadline_ns = inst->type->deadline_ms * MSEC_PER_NSEC;
    const long long period_ns = inst->type->period_ms * MSEC_PER_NSEC;

    // Anchor: Absolute time for first activation

    clock_gettime(CLOCK_MONOTONIC, &current_activation);
    while (!inst->stop) {
        struct timespec absolute_deadline = timespec_add_ns(current_activation, deadline_ns);

        clock_gettime(CLOCK_MONOTONIC, &start);
        if (inst->type->routine_fn) inst->type->routine_fn();
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (timespec_cmp(&end, &absolute_deadline) > 0) {
            const long long response_time = diff_ns(current_activation, end);
            printf("[Runtime] DEADLINE MISS: Task %s (ID %d) | Resp: %.2f ms > Limit: %ld ms\n",
                   inst->type->name, inst->id, response_time / 1000000.0, inst->type->deadline_ms);
        }

        current_activation = timespec_add_ns(current_activation, period_ns);

        while (!inst->stop) {
            const int ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &current_activation, NULL);
            if (ret == 0) break;
            if (ret == EINTR) break;
        }
    }
    return NULL;
}

void runtime_init(void) {
    pthread_mutex_lock(&pool_mutex);
    for (int i = 0; i < MAX_INSTANCES; i++) {
        pool[i].active = false;
        pool[i].id = -1;
    }
    atomic_store(&id_counter, 1);
    pthread_mutex_unlock(&pool_mutex);
}

int runtime_create_instance(const TaskType *type) {
    pthread_mutex_lock(&pool_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (!pool[i].active) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        pthread_mutex_unlock(&pool_mutex);
        return -1;
    }

    TaskInstance *inst = &pool[idx];
    inst->id = atomic_fetch_add(&id_counter, 1);
    inst->type = type;
    inst->stop = false;
    inst->active = true;

    pthread_attr_t attr;
    struct sched_param param;

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

    // DM: Higher frequency = Higher Priority
    // Mapped to range [1, 90] to leave room for system threads
    const int prio = 90 - (int) (type->deadline_ms / 100);
    param.sched_priority = (prio < 1) ? 1 : (prio > 90) ? 90 : prio;
    pthread_attr_setschedparam(&attr, &param);

    if (pthread_create(&inst->thread, &attr, thread_entry, inst) != 0) {
        inst->active = false;
        pthread_attr_destroy(&attr);
        pthread_mutex_unlock(&pool_mutex);
        fprintf(stderr, "[Runtime] Error creating thread. Check sudo/permissions.\n");
        return -1;
    }

    pthread_attr_destroy(&attr);
    const int id = inst->id;
    pthread_mutex_unlock(&pool_mutex);
    return id;
}

int runtime_stop_instance(const int id) {
    pthread_mutex_lock(&pool_mutex);
    int idx = -1;
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (pool[i].active && pool[i].id == id) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        pthread_mutex_unlock(&pool_mutex);
        return -1;
    }

    pool[idx].stop = true;

    // Interrupt nanosleep immediately to avoid waiting for the full period
    pthread_kill(pool[idx].thread, SIGUSR1);

    pthread_mutex_unlock(&pool_mutex);
    pthread_join(pool[idx].thread, NULL);

    pthread_mutex_lock(&pool_mutex);
    pool[idx].active = false;
    pool[idx].id = -1;
    pthread_mutex_unlock(&pool_mutex);
    return 0;
}

void runtime_cleanup(void) {
    pthread_mutex_lock(&pool_mutex);
    // Signal all threads to stop
    for (int i = 0; i < MAX_INSTANCES; i++) {
        if (pool[i].active) {
            pool[i].stop = true;
            pthread_kill(pool[i].thread, SIGUSR1);
        }
    }
    pthread_mutex_unlock(&pool_mutex);

    // Join all threads
    for (int i = 0; i < MAX_INSTANCES; i++) {
        pthread_t t = 0;
        bool active = false;
        pthread_mutex_lock(&pool_mutex);
        if (pool[i].active) {
            t = pool[i].thread;
            active = true;
        }
        pthread_mutex_unlock(&pool_mutex);

        if (active) pthread_join(t, NULL);
    }
}
