#include <string.h>

#include "logger.h"
#include "test_framework.h"

/* 로그를 임시 파일로 받아서, 찍힌 글자를 buf로 읽어 온다 */
static FILE *s_tmp = NULL;

static void capture_begin(void) {
    s_tmp = tmpfile();
    log_set_output(s_tmp);
}

static void capture_end(char *buf, size_t size) {
    rewind(s_tmp);
    size_t n = fread(buf, 1, size - 1u, s_tmp);
    buf[n] = '\0';
    fclose(s_tmp);
    s_tmp = NULL;
    log_set_output(NULL);
}

static void test_format(void) {
    char buf[256];
    log_set_level(LOG_LEVEL_INFO);
    log_set_time_ms(1800);
    capture_begin();
    LOG_INFO("차속 %u.%02u km/h", 16u, 0u);
    capture_end(buf, sizeof(buf));
    ASSERT_TRUE(strcmp("[   1.800s] INFO  차속 16.00 km/h\n", buf) == 0);
}

static void test_level_filter(void) {
    /* WARN으로 두면 DEBUG/INFO는 안 찍히고 WARN/ERROR만 찍힌다 */
    char buf[512];
    log_set_level(LOG_LEVEL_WARN);
    log_set_time_ms(0);
    capture_begin();
    LOG_DEBUG("debug");
    LOG_INFO("info");
    LOG_WARN("warn");
    LOG_ERROR("error");
    capture_end(buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "debug") == NULL);
    ASSERT_TRUE(strstr(buf, "info") == NULL);
    ASSERT_TRUE(strstr(buf, "WARN  warn") != NULL);
    ASSERT_TRUE(strstr(buf, "ERROR error") != NULL);
}

static void test_level_off(void) {
    char buf[64];
    log_set_level(LOG_LEVEL_OFF);
    capture_begin();
    LOG_ERROR("이것도 안 찍힘");
    capture_end(buf, sizeof(buf));
    ASSERT_EQ(0, strlen(buf));
}

static void test_debug_has_file_line(void) {
    /* DEBUG 로그에는 (파일:줄)이 붙는다 → 어디서 찍었는지 바로 알 수 있다 */
    char buf[256];
    log_set_level(LOG_LEVEL_DEBUG);
    log_set_time_ms(0);
    capture_begin();
    LOG_DEBUG("hello");
    capture_end(buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "(test_logger.c:") != NULL);
    ASSERT_TRUE(strstr(buf, "hello") != NULL);
}

static void test_parse_level(void) {
    LogLevel lv;
    ASSERT_TRUE(log_parse_level("debug", &lv));
    ASSERT_EQ(LOG_LEVEL_DEBUG, lv);
    ASSERT_TRUE(log_parse_level("warn", &lv));
    ASSERT_EQ(LOG_LEVEL_WARN, lv);
    ASSERT_TRUE(log_parse_level("off", &lv));
    ASSERT_EQ(LOG_LEVEL_OFF, lv);
    ASSERT_FALSE(log_parse_level("verbose", &lv));
    ASSERT_FALSE(log_parse_level("INFO", &lv));       /* 소문자만 허용 */
    ASSERT_FALSE(log_parse_level(NULL, &lv));
}

int main(void) {
    printf("[test_logger]\n");
    RUN_TEST(test_format);
    RUN_TEST(test_level_filter);
    RUN_TEST(test_level_off);
    RUN_TEST(test_debug_has_file_line);
    RUN_TEST(test_parse_level);
    return TEST_SUMMARY();
}
