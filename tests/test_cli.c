#include <string.h>

#include "cli.h"
#include "test_framework.h"

/* argv는 "char *" 배열이어야 해서, 문자열 상수를 쓸 수 있게 배열로 만든다 */
#define ARGC(a) ((int)(sizeof(a) / sizeof((a)[0])) - 1)   /* 끝의 NULL 빼고 */

static void test_long_options(void) {
    char *argv[] = {"bcm_sim", "--file", "a.log", "--log-level", "debug", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(argv), argv, &o, NULL));
    ASSERT_TRUE(strcmp("a.log", o.file) == 0);
    ASSERT_EQ(LOG_LEVEL_DEBUG, o.log_level);
}

static void test_short_options(void) {
    char *argv[] = {"bcm_sim", "-f", "b.log", "-l", "warn", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(argv), argv, &o, NULL));
    ASSERT_TRUE(strcmp("b.log", o.file) == 0);
    ASSERT_EQ(LOG_LEVEL_WARN, o.log_level);
}

static void test_default_level_is_info(void) {
    char *argv[] = {"bcm_sim", "--file", "c.log", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(argv), argv, &o, NULL));
    ASSERT_EQ(LOG_LEVEL_INFO, o.log_level);
}

static void test_order_does_not_matter(void) {
    char *argv[] = {"bcm_sim", "-l", "error", "--file", "d.log", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(argv), argv, &o, NULL));
    ASSERT_TRUE(strcmp("d.log", o.file) == 0);
    ASSERT_EQ(LOG_LEVEL_ERROR, o.log_level);
}

static void test_equals_form(void) {
    /* --file=경로 형태도 getopt_long이 받아준다 */
    char *argv[] = {"bcm_sim", "--file=e.log", "--log-level=off", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(argv), argv, &o, NULL));
    ASSERT_TRUE(strcmp("e.log", o.file) == 0);
    ASSERT_EQ(LOG_LEVEL_OFF, o.log_level);
}

static void test_help_and_version(void) {
    char *h[] = {"bcm_sim", "--help", NULL};
    char *h2[] = {"bcm_sim", "-h", NULL};
    char *v[] = {"bcm_sim", "--version", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_HELP, cli_parse(ARGC(h), h, &o, NULL));
    ASSERT_EQ(CLI_HELP, cli_parse(ARGC(h2), h2, &o, NULL));
    ASSERT_EQ(CLI_VERSION, cli_parse(ARGC(v), v, &o, NULL));
}

static void test_errors(void) {
    CliOptions o;
    char *no_file[]    = {"bcm_sim", NULL};                               /* --file 없음 */
    char *no_value[]   = {"bcm_sim", "--file", NULL};                     /* 값 없음 */
    char *bad_level[]  = {"bcm_sim", "--file", "a.log", "-l", "loud", NULL};
    char *unknown[]    = {"bcm_sim", "--file", "a.log", "--fast", NULL};  /* 모르는 옵션 */
    char *positional[] = {"bcm_sim", "a.log", NULL};                      /* M3-5 방식 (이제 안 됨) */
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(no_file), no_file, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(no_value), no_value, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(bad_level), bad_level, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(unknown), unknown, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(positional), positional, &o, NULL));
}

static void test_out_option(void) {
    char *a[] = {"bcm_sim", "--file", "a.log", "--out", "tx.log", NULL};
    char *b[] = {"bcm_sim", "-f", "a.log", "-o", "tx2.log", NULL};
    char *c[] = {"bcm_sim", "-f", "a.log", NULL};
    char *d[] = {"bcm_sim", "-f", "a.log", "--out", NULL};       /* 값 없음 */
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(a), a, &o, NULL));
    ASSERT_TRUE(strcmp("tx.log", o.out_file) == 0);
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(b), b, &o, NULL));
    ASSERT_TRUE(strcmp("tx2.log", o.out_file) == 0);
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(c), c, &o, NULL));
    ASSERT_TRUE(o.out_file == NULL);                              /* 안 주면 NULL */
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(d), d, &o, NULL));
}

static void test_iface_option(void) {
    char *a[] = {"bcm_sim", "--iface", "vcan0", "--dump", NULL};
    char *b[] = {"bcm_sim", "-i", "vcan0", "-d", "-l", "debug", NULL};
    char *both[] = {"bcm_sim", "-f", "a.log", "-i", "vcan0", "-d", NULL};   /* 둘 다 */
    char *none[] = {"bcm_sim", "-l", "info", NULL};                         /* 둘 다 없음 */
    char *dump_only[] = {"bcm_sim", "-f", "a.log", "--dump", NULL};         /* --dump는 --iface와 */
    char *iface_only[] = {"bcm_sim", "-i", "vcan0", "-o", "r.log", NULL};   /* --dump 없이 = 실시간 BCM (M5-5) */
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(a), a, &o, NULL));
    ASSERT_TRUE(strcmp("vcan0", o.iface) == 0);
    ASSERT_TRUE(o.dump);
    ASSERT_TRUE(o.file == NULL);
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(b), b, &o, NULL));
    ASSERT_EQ(LOG_LEVEL_DEBUG, o.log_level);
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(both), both, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(none), none, &o, NULL));
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(dump_only), dump_only, &o, NULL));
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(iface_only), iface_only, &o, NULL));
    ASSERT_FALSE(o.dump);
    ASSERT_TRUE(strcmp("r.log", o.out_file) == 0);
}

static void test_no_filter_option(void) {
    char *a[] = {"bcm_sim", "-i", "vcan0", "--no-filter", NULL};
    char *b[] = {"bcm_sim", "-i", "vcan0", NULL};
    char *file_nf[] = {"bcm_sim", "-f", "a.log", "-n", NULL};             /* 파일 모드에는 필터가 없다 */
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(a), a, &o, NULL));
    ASSERT_TRUE(o.no_filter);
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(b), b, &o, NULL));
    ASSERT_FALSE(o.no_filter);                                           /* 기본은 필터 켬 */
    ASSERT_EQ(CLI_ERROR, cli_parse(ARGC(file_nf), file_nf, &o, NULL));
}

static void test_parse_twice(void) {
    /* getopt 상태를 되돌리지 않으면 두 번째 해석이 틀어진다 (reset_getopt 확인) */
    char *a[] = {"bcm_sim", "--file", "first.log", NULL};
    char *b[] = {"bcm_sim", "--file", "second.log", NULL};
    CliOptions o;
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(a), a, &o, NULL));
    ASSERT_EQ(CLI_OK, cli_parse(ARGC(b), b, &o, NULL));
    ASSERT_TRUE(strcmp("second.log", o.file) == 0);
}

int main(void) {
    printf("[test_cli]\n");
    RUN_TEST(test_long_options);
    RUN_TEST(test_short_options);
    RUN_TEST(test_default_level_is_info);
    RUN_TEST(test_order_does_not_matter);
    RUN_TEST(test_equals_form);
    RUN_TEST(test_help_and_version);
    RUN_TEST(test_errors);
    RUN_TEST(test_out_option);
    RUN_TEST(test_iface_option);
    RUN_TEST(test_no_filter_option);
    RUN_TEST(test_parse_twice);
    return TEST_SUMMARY();
}
