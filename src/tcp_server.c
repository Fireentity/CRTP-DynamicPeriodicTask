#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include "tcp_server.h"
#include "supervisor.h"
#include "event.h"
#include <errno.h>

int tcp_server_init(TcpServer *svr, const int port) {
    // Initialize structure defaults
    svr->server_fd = -1;
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        svr->poll_fds[i].fd = -1;
        svr->poll_fds[i].events = 0;
        svr->client_buf_lens[i] = 0;
        memset(svr->client_buffers[i], 0, NET_BUFFER_SIZE);
    }

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return errno; // Return socket error
    }

    const int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(port)
    };

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0 ||
        listen(server_fd, BACKLOG_SIZE) < 0) {
        const int error = errno; // Capture bind/listen error
        close(server_fd);
        return error;
        }

    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL, 0) | O_NONBLOCK);
    svr->poll_fds[0].fd = server_fd;
    svr->poll_fds[0].events = POLLIN;
    svr->server_fd = server_fd;

    printf("[Net] Server listening on port %d\n", port);
    return 0; // Success
}


static void handle_line(Supervisor* spv, const int fd, char *line) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strlen(line) == 0) return;

    Event ev;
    if (event_parse(line, fd, &ev) != 0) {
        tcp_server_send_response(fd, "ERR Invalid Command\n");
        return;
    }


    if (ev.type == EV_SHUTDOWN) {
        tcp_server_send_response(fd, "OK Shutting Down\n");
    }

    // Push to supervisor queue. If full, reject immediately to prevent timeout.
    if (event_queue_push(&spv->queue, ev) != 0) {
        tcp_server_send_response(fd, "ERR System Busy\n");
    }
}

void tcp_server_poll(Supervisor* spv, TcpServer *svr) {
    struct pollfd *poll_fds = svr->poll_fds;

    const int ret = poll(poll_fds, MAX_CLIENTS + 1, 100);
    if (ret <= 0) return;

    if (poll_fds[0].revents & POLLIN) {
        const int new_sock = accept(poll_fds[0].fd, NULL, NULL);
        if (new_sock >= 0) {
            fcntl(new_sock, F_SETFL, fcntl(new_sock, F_GETFL, 0) | O_NONBLOCK);
            int added = 0;
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (poll_fds[i].fd == -1) {
                    poll_fds[i].fd = new_sock;
                    poll_fds[i].events = POLLIN;
                    svr->client_buf_lens[i] = 0;
                    added = 1;
                    printf("[Net] Client connected on FD %d\n", new_sock);
                    break;
                }
            }
            if (!added) {
                printf("[Net] Max clients reached, rejecting FD %d\n", new_sock);
                close(new_sock);
            }
        }
    }

    for (int i = 1; i <= MAX_CLIENTS; i++) {
        if (poll_fds[i].fd == -1) continue;
        if (!(poll_fds[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;

        const ssize_t n = recv(poll_fds[i].fd, svr->client_buffers[i], NET_BUFFER_SIZE - 1, 0);

        if (n <= 0) {
            printf("[Net] Client FD %d disconnected\n", poll_fds[i].fd);
            close(poll_fds[i].fd);
            poll_fds[i].fd = -1;
            svr->client_buf_lens[i] = 0;
            continue;
        }

        svr->client_buffers[i][n] = '\0';
        handle_line(spv, poll_fds[i].fd, svr->client_buffers[i]);
        svr->client_buf_lens[i] = 0;
    }
}
void tcp_server_send_response(const int client_fd, const char *msg) {
    if (client_fd >= 0) {
        char buf[NET_RESPONSE_BUF_SIZE];
        snprintf(buf, sizeof(buf), "[SERVER]: %s", msg);
        send(client_fd, buf, strlen(buf), MSG_NOSIGNAL);
    }
}

void tcp_server_cleanup(TcpServer *svr) {
    for (int i = 0; i <= MAX_CLIENTS; i++) {
        if (svr->poll_fds[i].fd != -1) {
            close(svr->poll_fds[i].fd);
            svr->poll_fds[i].fd = -1;
        }
    }
}
