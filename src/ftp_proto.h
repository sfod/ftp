#ifndef FTP_PROTO_INCLUDED_
#define FTP_PROTO_INCLUDED_

#include <stdint.h>
#include <stdlib.h>

#define FTP_HEADER_LEN_SIZE 4
#define FTP_MAX_HEADER_SIZE 4096

enum ftp_header_status {
    FTP_HEADER_NEED_SIZE,
    FTP_HEADER_NEED_NAME,
    FTP_HEADER_COMPLETED,
    FTP_HEADER_INVALID
};

struct ftp_proto_t {
    uint32_t hlen;
    uint32_t flen;
    char filename[FTP_MAX_HEADER_SIZE];
    int status;
};

int ftp_proto_parse_header(const char *s, size_t n, struct ftp_proto_t *proto,
        size_t *used);

#endif  /* FTP_PROTO_INCLUDED_ */
