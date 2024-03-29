#include "ftp_proto.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include <CUnit/Basic.h>

static char msg[4096];


static void print_proto(const struct ftp_proto_t *proto)
{
    printf("proto: %d\n\t'%s' (%d)\n\t'%s' (%d)\n", proto->status,
            proto->src_filename, proto->src_flen,
            proto->dst_filename, proto->dst_flen);
}

int init_suite()
{
    uint32_t n;
    char src_filename[50];
    char dst_filename[50];
    char content[256];
    char *s;

    strcpy(src_filename, "file");
    strcpy(dst_filename, "file_new");
    strcpy(content, "1234567890abcdefghijklmnopqrstuvwxyz");

    s = msg;

    /* store src_filename length */
    n = strlen(src_filename);
    n = htonl(n);
    memcpy(s, &n, sizeof(n));
    s += sizeof(n);

    /* store src_filename */
    strcpy(s, src_filename);
    s += strlen(src_filename);

    /* store dst_filename length */
    n = strlen(dst_filename);
    n = htonl(n);
    memcpy(s, &n, sizeof(n));
    s += sizeof(n);

    /* store dst_filename */
    strcpy(s, dst_filename);
    s += strlen(dst_filename);

    /* store file content */
    strcpy(s, content);

    return 0;
}

int clean_suite()
{
    return 0;
}

void test_parse_exact()
{
    struct ftp_proto_t proto;
    size_t used = 0;
    char *s = msg;
    int n;

    memset(&proto, 0, sizeof(proto));

    n = 20;
    CU_ASSERT_EQUAL_FATAL(1, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_COMPLETED, proto.status);
    CU_ASSERT_EQUAL_FATAL(20, used);
}

void test_parse_more()
{
    struct ftp_proto_t proto;
    size_t used = 0;
    char *s = msg;
    int n;

    memset(&proto, 0, sizeof(proto));

    n = 22;
    CU_ASSERT_EQUAL_FATAL(1, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_COMPLETED, proto.status);
    CU_ASSERT_EQUAL_FATAL(20, used);
}

void test_parse_parts()
{
    struct ftp_proto_t proto;
    size_t used = 0;
    char *s = msg;
    int n;

    memset(&proto, 0, sizeof(proto));

    n = 2;
    used = 0;
    CU_ASSERT_EQUAL_FATAL(0, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_NEED_SRC_LEN, proto.status);
    s += n;

    n = 4;
    used = 0;
    CU_ASSERT_EQUAL_FATAL(0, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_NEED_SRC_NAME, proto.status);
    s += n;

    n = 4;
    used = 0;
    CU_ASSERT_EQUAL_FATAL(0, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_NEED_DST_LEN, proto.status);
    s += n;

    n = 4;
    used = 0;
    CU_ASSERT_EQUAL_FATAL(0, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_NEED_DST_NAME, proto.status);
    s += n;

    n = 10;
    used = 0;
    CU_ASSERT_EQUAL_FATAL(1, ftp_proto_parse_header(s, n, &proto, &used));
    CU_ASSERT_EQUAL_FATAL(FTP_HEADER_COMPLETED, proto.status);
    s += n;

    CU_ASSERT_EQUAL_FATAL(6, used);
}

int main()
{
    CU_pSuite pSuite = NULL;

    if (CU_initialize_registry() != CUE_SUCCESS) {
        return CU_get_error();
    }

    pSuite = CU_add_suite("suite", init_suite, clean_suite);
    if (pSuite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    if ((CU_add_test(pSuite, "parsing test", test_parse_exact) == NULL)
            || (CU_add_test(pSuite, "parsing test", test_parse_more) == NULL)
            || (CU_add_test(pSuite, "parsing test", test_parse_parts) == NULL)) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    CU_cleanup_registry();
    return CU_get_error();
}
