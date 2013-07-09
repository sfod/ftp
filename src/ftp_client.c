#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "ftp.h"


#define FTP_SOCK_BUF_SIZE 4096


static int transfer_file(int cl_fd, const char *filename);
static int send_header(int cl_fd, const char *filename);

int main(int argc, char **argv)
{
    int cl_fd;
    struct sockaddr_in servaddr;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <host> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if ((cl_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(FTP_PORT);
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (connect(cl_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        return EXIT_FAILURE;
    }

    if (transfer_file(cl_fd, argv[2]) < 0) {
        fprintf(stderr, "failed to transfer %s\n", argv[2]);
    }

    close(cl_fd);

    return EXIT_SUCCESS;
}

static int transfer_file(int cl_fd, const char *filename)
{
    FILE *fh;
    char buf[FTP_SOCK_BUF_SIZE];
    size_t rb;
    int rc;


    if (send_header(cl_fd, filename) < 0) {
        fprintf(stderr, "failed to send header\n");
        return -1;
    }

    fh = fopen(filename, "r");
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

static int send_header(int cl_fd, const char *filename)
{
    uint32_t n;

    n = strlen(filename);
    n = htonl(n);

    if (send(cl_fd, &n, sizeof(n), 0) < 0) {
        perror("send");
        return -1;
    }

    if (send(cl_fd, filename, strlen(filename), 0) < 0) {
        perror("send");
        return -1;
    }

    return 0;
}
