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

#include "ftp.h"
#include "ftp_proto.h"


#define FTP_INF_TIME -1
#define FTP_AVAILABLE_FD -1
#define FTP_OPEN_MAX 1024

#define FTP_SOCK_BUF_SIZE 4096


struct file_t {
    FILE *fh;
};


static void main_loop(int listen_fd);
static int process_client_data(char *buf, size_t n, struct ftp_proto_t *proto,
        struct file_t *file);
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
    nfds_t nfds = FTP_OPEN_MAX;
    struct pollfd fds[FTP_OPEN_MAX];
    struct file_t files[FTP_OPEN_MAX];
    struct ftp_proto_t protos[FTP_OPEN_MAX];
    int conn_fd;
    int nready;
    socklen_t clilen;
    struct sockaddr_in cliaddr;
    char buf[FTP_SOCK_BUF_SIZE];
    ssize_t n;
    int i;

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    for (i = 1; i < FTP_OPEN_MAX; ++i) {
        fds[i].fd = FTP_AVAILABLE_FD;
    }

    memset(files, 0, sizeof(files));

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
                    break;
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
                    memset(protos + i, 0, sizeof(protos[i]));
                }
                else if (n == 0) {
                    printf("client disconnected\n");
                    fds[i].fd = FTP_AVAILABLE_FD;
                    memset(protos + i, 0, sizeof(protos[i]));
                    fclose(files[i].fh);
                }
                else if (process_client_data(buf, n, protos + i, files + i) < 0) {
                    fprintf(stderr, "failed to parse client data\n");
                    close(fds[i].fd);
                    fds[i].fd = FTP_AVAILABLE_FD;
                }

                if (--nready <= 0) {
                    break;
                }
            }
        }
    }
}

static int process_client_data(char *buf, size_t n, struct ftp_proto_t *proto,
        struct file_t *file)
{
    size_t used;
    int rc;

    switch (proto->status) {
    case FTP_HEADER_COMPLETED:
        if (fwrite(buf, sizeof(*buf), n, file->fh) != n) {
            fprintf(stderr, "fwrite failed\n");
            return -1;
        }
        break;
    default:
        if ((rc = ftp_proto_parse_header(buf, n, proto, &used)) < 0) {
            fprintf(stderr, "failed to parse protocol header\n");
            memset(proto, 0, sizeof(*proto));
            return -1;
        }
        else if (rc == 1) {
            printf("writing to %s\n", proto->dst_filename);
            if ((file->fh = fopen(proto->dst_filename, "w")) == NULL) {
                perror("fopen");
                return -1;
            }
            if ((n - used != 0)
                    && (fwrite(buf, sizeof(*buf), n - used, file->fh) != n - used)) {
                fprintf(stderr, "fwrite failed\n");
                return -1;
            }
        }
    }

    return 0;
}

static int init_listen_fd(int *listen_fd)
{
    int fd;
    int y;
    struct sockaddr_in servaddr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return -1;
    }

    y = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &y, sizeof(y)) < 0) {
        perror("setsockopt");
    }

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
