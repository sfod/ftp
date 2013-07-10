#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ftp.h"


#define FTP_SOCK_BUF_SIZE 4096


static int transfer_file(int cl_fd, const char *src_file, const char *dst_file);
static int send_header(int cl_fd, const char *src_file, const char *dst_file);

int main(int argc, char **argv)
{
    int cl_fd;
    struct sockaddr_in servaddr;
    const char *host;
    const char *src_file;
    const char *dst_file;

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <host> <src_file> <dst_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    host = argv[1];
    src_file = argv[2];
    dst_file = argv[3];

    if ((cl_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(FTP_PORT);
    if (inet_pton(AF_INET, host, &servaddr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: '%s'\n", host);
        return EXIT_FAILURE;
    }

    if (connect(cl_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        return EXIT_FAILURE;
    }

    if (transfer_file(cl_fd, src_file, dst_file) < 0) {
        fprintf(stderr, "failed to transfer file %s\n", src_file);
    }

    close(cl_fd);

    return EXIT_SUCCESS;
}

static int transfer_file(int cl_fd, const char *src_file, const char *dst_file)
{
    FILE *fh;
    char buf[FTP_SOCK_BUF_SIZE];
    size_t rb;
    int rc;


    if (send_header(cl_fd, src_file, dst_file) < 0) {
        fprintf(stderr, "failed to send header\n");
        return -1;
    }

    fh = fopen(src_file, "r");
    if (fh == NULL) {
        perror("fopen");
        return -1;
    }

    rc = 0;
    while ((rb = fread(buf, sizeof(*buf), sizeof(buf), fh)) != 0) {
        if (send(cl_fd, buf, rb, 0) < 0) {
            perror("send");
            rc = -1;
            break;
        }
    }

    if ((rc == 0) && !feof(fh)) {
        fprintf(stderr, "failed to read from file\n");
        rc = -1;
    }

    fclose(fh);
    return rc;
}

static int send_header(int cl_fd, const char *src_file, const char *dst_file)
{
    uint32_t n;

    n = strlen(src_file);
    n = htonl(n);
    if (send(cl_fd, &n, sizeof(n), 0) < 0) {
        perror("send");
        return -1;
    }
    if (send(cl_fd, src_file, strlen(src_file), 0) < 0) {
        perror("send");
        return -1;
    }

    n = strlen(dst_file);
    n = htonl(n);
    if (send(cl_fd, &n, sizeof(n), 0) < 0) {
        perror("send");
        return -1;
    }
    if (send(cl_fd, dst_file, strlen(dst_file), 0) < 0) {
        perror("send");
        return -1;
    }

    return 0;
}
