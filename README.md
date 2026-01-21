
## Concurrent and Real-Time Programming

This project implements a soft/hard real-time supervisor designed to dynamically accept task requests via a TCP interface. Upon receiving a request, the system verifies schedulability using Response Time Analysis (RTA) before executing accepted tasks with strict timing guarantees using `SCHED_FIFO`.

The architecture prioritizes precision and safety. It ensures zero-accumulated drift by utilizing `clock_nanosleep` with `TIMER_ABSTIME`. The network core handles I/O multiplexing through a single-threaded `poll()` implementation, robustly managing TCP fragmentation and buffer overflows. Internally, the supervisor relies on a thread-safe queue and atomic operations to manage concurrency and graceful shutdowns effectively.

## Building and Running

### Prerequisites

To build and run this project, you need a Linux environment (required for `SCHED_FIFO` and processor affinity), CMake (version 3.16+), a GCC/Clang compiler, and Python 3 for the test suite.

### Compilation and Execution

You can compile the project using the provided shell script. Note that **root privileges are required** to run the executable, as it must set real-time thread priorities.

```bash
# Compile
bash build.sh

# Run
sudo ./build/dynamic_periodic_task

```

### Automated Testing

The included test suite handles startup timing automatically and is compatible with Valgrind for memory analysis.

```bash
sudo bash build.sh --test

```

## Communication Protocol

The supervisor listens for ASCII commands on **port 8080** via Telnet or Netcat. The supported commands are detailed below:

| Command | Arguments | Description |
| --- | --- | --- |
| `ACTIVATE` | `<task_name>` | Requests the execution of a task. Returns `ID=<id>` on success. |
| `DEACTIVATE` | `<id>` | Stops a specific running instance. |
| `LIST` | N/A | Displays all currently active task instances. |
| `INFO` | N/A | Returns the task catalog and current system capacity. |
| `SHUTDOWN` | N/A | Gracefully terminates the server and all worker threads. |
