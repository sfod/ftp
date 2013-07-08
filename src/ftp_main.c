#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <poll.h>


#define FTP_INF_TIME -1
#define FTP_AVAILABLE_FD -1
#define FTP_OPEN_MAX 1024

#define FTP_PORT 13782


static void main_loop(int listen_fd);
static int init_listen_fd(int *listen_fd);
static int set_nonblocking_mode(int fd);


int main()
{
    int listen_fd;
    if (init_listen_fd(&listen_fd) < 0) {
        fprintf(stderr, "failed to initialize socket\n");
        return EXIT_FAILURE;
    }

    main_loop(listen_fd);

    return EXIT_SUCCESS;
}

static void main_loop(int listen_fd)
{
    nfds_t nfds = 1;
    struct pollfd fds[FTP_OPEN_MAX];
    int conn_fd;
    int nready;
    socklen_t clilen;
    struct sockaddr_in cliaddr;
    char buf[4096];
    ssize_t n;
    int i;

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    for (i = 1; i < FTP_OPEN_MAX; ++i) {
        fds[i].fd = FTP_AVAILABLE_FD;
    }

    while (1) {
        nready = poll(fds, nfds, FTP_INF_TIME);
        if (nready < 0) {
            perror("poll");
            exit(EXIT_FAILURE);
        }

        /* got new connection */
        if (fds[0].revents & POLLIN) {
            clilen = sizeof(cliaddr);
            conn_fd = accept(fds[0].fd, (struct sockaddr *) &cliaddr, &clilen);

            printf("new client connected\n");

            for (i = 1; i < FTP_OPEN_MAX; ++i) {
                if (fds[i].fd == FTP_AVAILABLE_FD) {
                    fds[i].fd = conn_fd;
                    fds[i].events = POLLIN;
                }
            }

            if (i == FTP_OPEN_MAX) {
                fprintf(stderr, "too many clients\n");
            }

            if (--nready <= 0) {
                continue;
            }
        }

        /* check all clients for data */
        for (i = 1; i < FTP_OPEN_MAX; ++i) {
            if (fds[i].fd == FTP_AVAILABLE_FD) {
                continue;
            }

            if (fds[i].revents & POLLIN) {
                if ((n = recv(fds[i].fd, buf, sizeof(buf), 0)) < 0) {
                    perror("recv");
                    close(fds[i].fd);
                    fds[i].fd = FTP_AVAILABLE_FD;
                }
                else if (n == 0) {
                    printf("client disconnected\n");
                    fds[i].fd = FTP_AVAILABLE_FD;
                }
                else {
                    printf("read %ld bytes\n", n);
                }
            }

            if (--nready <= 0) {
                break;
            }
        }
    }
}

static int init_listen_fd(int *listen_fd)
{
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(FTP_PORT);
    if (bind(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        return -1;
    }

    if (set_nonblocking_mode(fd) < 0) {
        return -1;
    }

    if (listen(fd, 5) < 0) {
        perror("listen");
        return -1;
    }

    *listen_fd = fd;
    return 0;
}

static int set_nonblocking_mode(int fd)
{
    int flags;
    if ((fd < 0)
            || ((flags = fcntl(fd, F_GETFL, 0)) < 0)
            || (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)) {
        return -1;
    }
    return 0;
}
