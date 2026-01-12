#ifndef NET_CORE_H
#define NET_CORE_H
#include "constants.h"
#include "supervisor.h"

// In net_core.h

typedef struct {
    struct pollfd poll_fds[MAX_CLIENTS + 1];
    char client_buffers[MAX_CLIENTS + 1][NET_BUFFER_SIZE];
    long client_buf_lens[MAX_CLIENTS + 1];
    int server_fd;
} TcpServer;

/**
 * Initializes server socket, binds port, and sets non-blocking mode.
 * @return 0 on success, -1 on failure.
 */
int tcp_server_init(TcpServer *svr, int port);

/**
 * Handles I/O multiplexing (poll). Accepts connections and reads data.
 * Passes complete lines to the event parser.
 */
void tcp_server_poll(Supervisor* spv, TcpServer *svr);

/**
 * Sends a response to a specific client. Safe against broken pipes.
 */
void tcp_server_send_response(int client_fd, const char *msg);

/**
 * Closes all sockets.
 */
void tcp_server_cleanup(TcpServer *svr);

#endif
