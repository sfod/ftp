#include "ftp_proto.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

int main()
{
    char s[4096];
    uint32_t n;
    char filename[50];
    char content[256];
    int rc;
    struct ftp_proto_t proto;
    size_t used;

    strcpy(filename, "/tmp/filename");
    strcpy(content, "1234567890abcdefghijklmnopqrstuvwxyz");

    /* store filename */
    n = strlen(filename);
    strcpy(s + sizeof(n), filename);
    /* store file content */
    strcpy(s + sizeof(n) + n, content);
    /* store filename length */
    n = htonl(n);
    memcpy(s, &n, sizeof(n));


    if ((rc = ftp_proto_parse_header(s, 2, &proto, &used)) < 0) {
        fprintf(stderr, "failed to parse message\n");
        return EXIT_FAILURE;
    }
    else if (rc == 1) {
        fprintf(stderr, "parsed incomplete message\n");
        return EXIT_FAILURE;
    }
    if (proto.status != FTP_HEADER_NEED_SIZE) {
        fprintf(stderr, "wrong proto status: %d\n", proto.status);
        return EXIT_FAILURE;
    }


    if ((rc = ftp_proto_parse_header(s + 2, 8, &proto, &used)) < 0) {
        fprintf(stderr, "failed to parse message\n");
        return EXIT_FAILURE;
    }
    else if (rc == 1) {
        fprintf(stderr, "parsed incomplete message\n");
        printf("proto.flen is %d\n", proto.flen);
        printf("proto.filename is '%s'\n", proto.filename);
        return EXIT_FAILURE;
    }
    if (proto.status != FTP_HEADER_NEED_NAME) {
        fprintf(stderr, "wrong proto status: %d\n", proto.status);
        return EXIT_FAILURE;
    }
    printf("proto.flen is %d\n", proto.flen);
    printf("proto.filename is '%s'\n", proto.filename);


    if ((rc = ftp_proto_parse_header(s + 10, 10, &proto, &used)) < 0) {
        fprintf(stderr, "failed to parse message\n");
        return EXIT_FAILURE;
    }
    else if (rc == 0) {
        fprintf(stderr, "did not parse complete message\n");
        return EXIT_FAILURE;
    }
    printf("proto.filename is '%s'\n", proto.filename);
    printf("used = %ld\n", used);

    return EXIT_SUCCESS;
}
