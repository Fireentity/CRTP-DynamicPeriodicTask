#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <signal.h>
#include "supervisor.h"
#include "tcp_server.h"
#include "constants.h"
#include "task_runtime.h"
#include "task_config.h"

// Context to pass multiple arguments to the network thread
typedef struct {
    Supervisor *sv;
    TcpServer *server;
} NetworkContext;

// Empty handler to interrupt blocking syscalls (e.g., nanosleep)
static void sigusr1_handler(const int signum) { (void) signum; }

static void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sa.sa_flags = 0; // No SA_RESTART: ensure blocking calls return EINTR
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
}

static void *network_entry(void *arg) {
    const NetworkContext *ctx = arg;
    while (atomic_load(&ctx->sv->running)) {
        tcp_server_poll(ctx->sv, ctx->server);
    }
    return NULL;
}

static void *supervisor_entry(void *arg) {
    Supervisor *sv = arg;
    supervisor_loop(sv);
    atomic_store(&sv->running, false);
    return NULL;
}

static void set_fifo_priority(pthread_attr_t *attr, const int prio) {
    const struct sched_param param = {.sched_priority = prio};
    pthread_attr_init(attr);
    pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(attr, SCHED_FIFO);
    pthread_attr_setschedparam(attr, &param);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0); // Disable buffering for real-time logs
    setup_signals();

    if (geteuid() != 0) {
        fprintf(stderr, "WARNING: Not running as root. SCHED_FIFO tasks may fail.\n");
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(CPU_NUMBER, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        perror("[Main] Failed to set CPU affinity");
    }

    /* Initialize ALL internal subsystems before creating threads or opening sockets. */
    Supervisor supervisor;
    supervisor_init(&supervisor);
    tasks_config_init(&tasks_config); // Blocking CPU calibration
    runtime_init();

    // Open network port only after internals are ready
    TcpServer server;
    const int net_err = tcp_server_init(&server, SERVER_PORT);
    if (net_err != 0) {
        return EXIT_FAILURE;
    }

    pthread_t net_thread, sv_thread;
    pthread_attr_t net_attr, sv_attr;

    // Priorities: Network (99) > Supervisor (98) > Tasks (max 90)
    set_fifo_priority(&net_attr, 99);
    set_fifo_priority(&sv_attr, 98);

    if (pthread_create(&sv_thread, &sv_attr, supervisor_entry, &supervisor) != 0) {
        fprintf(stderr, "[Main] CRITICAL: Failed to create Supervisor thread\n");
        return EXIT_FAILURE;
    }

    NetworkContext net_ctx = { .sv = &supervisor, .server = &server };

    if (pthread_create(&net_thread, &net_attr, network_entry, &net_ctx) != 0) {
        fprintf(stderr, "[Main] CRITICAL: Failed to create Network thread\n");
        atomic_store(&supervisor.running, false);
        pthread_join(sv_thread, NULL);
        return EXIT_FAILURE;
    }

    pthread_join(sv_thread, NULL);
    pthread_join(net_thread, NULL);

    tcp_server_cleanup(&server);
    runtime_cleanup();

    return EXIT_SUCCESS;
}