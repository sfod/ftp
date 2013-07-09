#include "ftp_proto.h"

#include <arpa/inet.h>
#include <string.h>


int ftp_proto_parse_header(const char *s, size_t n, struct ftp_proto_t *proto,
        size_t *used)
{
    const char *ts = s;
    uint32_t tn;

    switch (proto->status) {
    /* received only part of the header */
    case FTP_HEADER_NEED_SIZE:
        if (proto->hlen + n < FTP_HEADER_LEN_SIZE) {
            memcpy((char *) &proto->flen + proto->hlen, s, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_SIZE;
            return 0;
        }
        tn = FTP_HEADER_LEN_SIZE - proto->hlen;
        memcpy((char *) &proto->flen + proto->hlen, s, tn);
        proto->flen = ntohl(proto->flen);
        if (proto->flen > FTP_MAX_HEADER_SIZE - FTP_HEADER_LEN_SIZE) {
            return -1;
        }
        ts = s + tn;
        n -= tn;
        proto->hlen = 0;
        /* fall through */
    case FTP_HEADER_NEED_NAME:
        if (n + proto->hlen < proto->flen) {
            memcpy(proto->filename + proto->hlen, ts, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_NAME;
            return 0;
        }
        memcpy(proto->filename + proto->hlen, ts, proto->flen - proto->hlen);
        proto->status = FTP_HEADER_COMPLETED;
        *used = proto->flen - proto->hlen;
        /* fall through */
    case FTP_HEADER_COMPLETED:
        return 1;
    default:
        proto->status = FTP_HEADER_INVALID;
        return -1;
    }
}
