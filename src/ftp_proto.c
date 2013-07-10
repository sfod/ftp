#include "ftp_proto.h"

#include <arpa/inet.h>
#include <string.h>


int ftp_proto_parse_header(const char *s, size_t n, struct ftp_proto_t *proto,
        size_t *used)
{
    const char *ts = s;
    uint32_t tused;

    switch (proto->status) {
    case FTP_HEADER_NEED_SRC_LEN:
        if (proto->hlen + n < FTP_HEADER_LEN_SIZE) {
            memcpy((char *) &proto->src_flen + proto->hlen, ts, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_SRC_LEN;
            return 0;
        }
        tused = FTP_HEADER_LEN_SIZE - proto->hlen;
        memcpy((char *) &proto->src_flen + proto->hlen, ts, tused);
        proto->src_flen = ntohl(proto->src_flen);
        if (proto->src_flen > FTP_MAX_HEADER_SIZE - FTP_HEADER_LEN_SIZE) {
            return -1;
        }
        ts += tused;
        n -= tused;
        proto->hlen = 0;
        *used += tused;
        /* fall through */
    case FTP_HEADER_NEED_SRC_NAME:
        if (n + proto->hlen < proto->src_flen) {
            memcpy(proto->src_filename + proto->hlen, ts, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_SRC_NAME;
            return 0;
        }
        tused = proto->src_flen - proto->hlen;
        memcpy(proto->src_filename + proto->hlen, ts, tused);
        ts += tused;
        n -= tused;
        proto->hlen = 0;
        *used += tused;
        /* fall through */
    case FTP_HEADER_NEED_DST_LEN:
        if (proto->hlen + n < FTP_HEADER_LEN_SIZE) {
            memcpy((char *) &proto->dst_flen + proto->hlen, ts, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_DST_LEN;
            return 0;
        }
        tused = FTP_HEADER_LEN_SIZE - proto->hlen;
        memcpy((char *) &proto->dst_flen + proto->hlen, ts, tused);
        proto->dst_flen = ntohl(proto->dst_flen);
        if (proto->dst_flen > FTP_MAX_HEADER_SIZE - FTP_HEADER_LEN_SIZE) {
            return -1;
        }
        ts += tused;
        n -= tused;
        proto->hlen = 0;
        *used += tused;
        /* fall through */
    case FTP_HEADER_NEED_DST_NAME:
        if (n + proto->hlen < proto->dst_flen) {
            memcpy(proto->dst_filename + proto->hlen, ts, n);
            proto->hlen += n;
            proto->status = FTP_HEADER_NEED_DST_NAME;
            return 0;
        }
        tused = proto->dst_flen - proto->hlen;
        memcpy(proto->dst_filename + proto->hlen, ts, tused);
        *used += tused;
        /* fall through */
    case FTP_HEADER_COMPLETED:
        proto->status = FTP_HEADER_COMPLETED;
        return 1;
    default:
        proto->status = FTP_HEADER_INVALID;
        return -1;
    }
}
