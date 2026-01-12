#include <math.h>
#include "constants.h"
#include "task_config.h"

void task_run(const double i) {
    const volatile double r = sqrt(i) * 0.001 + sin(i / 1000.0);
    (void) r;
}

void task_run_for(const TasksConfig* config, const long ms) {
    const unsigned long long max = config->loops_per_ms * ms;
    for (unsigned long long i = 0; i < max; i++) task_run((double) i);
}