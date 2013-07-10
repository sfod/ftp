#define _POSIX_C_SOURCE 200112L

#include <errno.h>
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

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include <sys/queue.h>

#include "ftp.h"
#include "ftp_proto.h"


#define FTP_POLL_TIMEOUT 1000
#define FTP_AVAILABLE_FD -1
#define FTP_OPEN_MAX 1024
#define FTP_WORKERS_NUMBER 10

#define FTP_SOCK_BUF_SIZE 4096


struct file_t {
    FILE *fh;
};

struct req_t {
    int cl_idx;
    struct file_t file;
    char *s;
    size_t n;
    TAILQ_ENTRY(req_t) entries;
};

struct cl_t {
    int idx;
    TAILQ_ENTRY(cl_t) entries;
};


static void main_loop(int listen_fd);
static void *pthread_sighandler(void *p);
static void *pthread_process_req(void *p);
static void invalidate_client(int cl_idx);
static void clean_client(struct pollfd *fd, struct file_t *file,
        struct ftp_proto_t *proto);
static int process_client_data(char *buf, size_t n, struct ftp_proto_t *proto,
        struct file_t *file, int cl_idx);
static void write_data(const char *s, size_t n, const struct file_t *file,
        int cl_idx);
static int init_listen_fd(int *listen_fd);
static int set_nonblocking_mode(int fd);


pthread_mutex_t g_mutex_req;
pthread_mutex_t g_mutex_cl;
sem_t g_sem_req;

TAILQ_HEAD(, req_t) g_req_queue;
TAILQ_HEAD(, cl_t) g_cl_queue;

volatile int g_is_running = 1;

int main()
{
    TAILQ_INIT(&g_req_queue);
    TAILQ_INIT(&g_cl_queue);

    if (pthread_mutex_init(&g_mutex_req, NULL) < 0) {
        perror("pthread_mutex_init");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&g_mutex_cl, NULL) < 0) {
        perror("pthread_mutex_init");
        return EXIT_FAILURE;
    }

    if (sem_init(&g_sem_req, 0, 0) < 0) {
        perror("sem_init");
        return EXIT_FAILURE;
    }

    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    pthread_t sig_tid;
    if (pthread_create(&sig_tid, NULL, &pthread_sighandler, (void *) &set) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }
    if (pthread_detach(sig_tid) < 0) {
        perror("pthread_detach");
        return EXIT_FAILURE;
    }

    pthread_t req_tids[FTP_WORKERS_NUMBER];
    for (int i = 0; i < FTP_WORKERS_NUMBER; ++i) {
        if (pthread_create(req_tids + i, NULL, pthread_process_req, NULL) < 0) {
            perror("pthread_create");
            return EXIT_FAILURE;
        }
        if (pthread_detach(req_tids[i]) < 0) {
            perror("pthread_detach");
            return EXIT_FAILURE;
        }
    }

    int listen_fd;
    if (init_listen_fd(&listen_fd) < 0) {
        fprintf(stderr, "failed to initialize socket\n");
        return EXIT_FAILURE;
    }

    main_loop(listen_fd);

    return EXIT_SUCCESS;
}

static void *pthread_sighandler(void *p)
{
    sigset_t *set = (sigset_t *) p;
    int sig;

    while (g_is_running) {
        if (sigwait(set, &sig) < 0) {
            continue;
        }

        switch (sig) {
        case SIGTERM:
        case SIGINT:
            fprintf(stderr, "shutting down server\n");
            g_is_running = 0;
            break;
        default:
            fprintf(stderr, "got unsupported signal, ignoring\n");
            break;
        }
    }

    return NULL;
}

static void *pthread_process_req(void *p)
{
    struct req_t *req;
    struct timespec ts_sem = {0, 0};

    while (g_is_running) {
        ts_sem.tv_sec = time(NULL) + 1;
        if (sem_timedwait(&g_sem_req, &ts_sem) == -1) {
            if (errno == EINTR || errno == ETIMEDOUT) {
                continue;
            }
        }

        pthread_mutex_lock(&g_mutex_req);
        req = TAILQ_FIRST(&g_req_queue);
        TAILQ_REMOVE(&g_req_queue, req, entries);
        pthread_mutex_unlock(&g_mutex_req);

        if (fwrite(req->s, sizeof(*req->s), req->n, req->file.fh)
                != req->n * sizeof(*req->s)) {
            fprintf(stderr, "failed to write data\n");
            invalidate_client(req->cl_idx);
        }

        free(req->s);
        free(req);
    }

    return NULL;
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
    struct cl_t *cl;

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;
    for (i = 1; i < FTP_OPEN_MAX; ++i) {
        fds[i].fd = FTP_AVAILABLE_FD;
    }

    memset(files, 0, sizeof(files));

    while (g_is_running) {
        /* check if one of client's transferring failed */
        if (!TAILQ_EMPTY(&g_cl_queue)) {
            pthread_mutex_lock(&g_mutex_cl);
            TAILQ_FOREACH(cl, &g_cl_queue, entries) {
                clean_client(fds + cl->idx, files + cl->idx, protos + cl->idx);
            }
            pthread_mutex_unlock(&g_mutex_cl);
        }

        nready = poll(fds, nfds, FTP_POLL_TIMEOUT);
        if (nready == 0) {
            continue;
        }
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
                    clean_client(fds + i, files + i, protos + i);
                }
                else if (n == 0) {
                    printf("client disconnected\n");
                    clean_client(fds + i, files + i, protos + i);
                }
                else if (process_client_data(buf, n, protos + i, files + i, i) < 0) {
                    fprintf(stderr, "failed to parse client data\n");
                    clean_client(fds + i, files + i, protos + i);
                }

                if (--nready <= 0) {
                    break;
                }
            }
        }
    }

    shutdown(listen_fd, SHUT_RDWR);
}

static void clean_client(struct pollfd *fd, struct file_t *file,
        struct ftp_proto_t *proto)
{
    close(fd->fd);
    fd->fd = FTP_AVAILABLE_FD;
    if (file->fh != NULL) {
        fclose(file->fh);
        file->fh = NULL;
    }
    memset(proto, 0, sizeof(*proto));
}

static void invalidate_client(int cl_idx)
{
    struct cl_t *cl = malloc(sizeof(struct cl_t));
    cl->idx = cl_idx;
    pthread_mutex_lock(&g_mutex_cl);
    TAILQ_INSERT_TAIL(&g_cl_queue, cl, entries);
    pthread_mutex_unlock(&g_mutex_cl);
}

static int process_client_data(char *buf, size_t n, struct ftp_proto_t *proto,
        struct file_t *file, int cl_idx)
{
    size_t used = 0;
    int rc;

    switch (proto->status) {
    case FTP_HEADER_COMPLETED:
        write_data(buf, n, file, cl_idx);
        break;
    default:
        if ((rc = ftp_proto_parse_header(buf, n, proto, &used)) < 0) {
            fprintf(stderr, "failed to parse protocol header\n");
            memset(proto, 0, sizeof(*proto));
            return -1;
        }
        else if (rc == 1) {
            if ((file->fh = fopen(proto->dst_filename, "w")) == NULL) {
                perror("fopen");
                return -1;
            }
            if (n - used != 0) {
                write_data(buf + used, n - used, file, cl_idx);
                return -1;
            }
        }
    }

    return 0;
}

static void write_data(const char *s, size_t n, const struct file_t *file,
        int cl_idx)
{
    struct req_t *req = malloc(sizeof(struct req_t));
    req->cl_idx = cl_idx;
    req->file = *file;
    req->s = malloc(n);
    req->n = n;
    memcpy(req->s, s, n);

    pthread_mutex_lock(&g_mutex_req);
    TAILQ_INSERT_TAIL(&g_req_queue, req, entries);
    pthread_mutex_unlock(&g_mutex_req);

    sem_post(&g_sem_req);
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
