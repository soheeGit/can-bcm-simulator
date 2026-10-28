#include <string.h>

#include "log_parser.h"
#include "test_framework.h"

#define ASSERT_BYTES_EQ(expected, actual, n) ASSERT_TRUE(memcmp((expected), (actual), (n)) == 0)

/* ========== 정상 예시 (docs/log-format.md 3번) ========== */

static void test_ok_full_frame(void) {
    CanFrame f;
    uint64_t ts = 0;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.100000) vcan0 100#1027000000000000", &f, &ts));
    ASSERT_EQ(0x100, f.id);
    ASSERT_EQ(8, f.dlc);
    const uint8_t expected[8] = {0x10, 0x27, 0, 0, 0, 0, 0, 0};
    ASSERT_BYTES_EQ(expected, f.data, 8);
    ASSERT_EQ(100000, ts);                              /* 0.1초 = 100000us */
}

static void test_ok_ign(void) {
    CanFrame f;
    uint64_t ts = 1;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.000000) vcan0 300#0203000000000000", &f, &ts));
    ASSERT_EQ(0x300, f.id);
    ASSERT_EQ(0x02, f.data[0]);
    ASSERT_EQ(0x03, f.data[1]);
    ASSERT_EQ(0, ts);
}

static void test_ok_dlc_1(void) {
    CanFrame f;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.200000) vcan0 200#11", &f, NULL));
    ASSERT_EQ(0x200, f.id);
    ASSERT_EQ(1, f.dlc);                                /* 2글자 = 1바이트 */
    ASSERT_EQ(0x11, f.data[0]);
}

static void test_ok_dlc_0(void) {
    CanFrame f;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.300000) vcan0 555#", &f, NULL));
    ASSERT_EQ(0x555, f.id);
    ASSERT_EQ(0, f.dlc);                                /* 데이터 없음도 문법상 정상 */
}

static void test_ok_real_timestamp_lowercase(void) {
    CanFrame f;
    uint64_t ts = 0;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(1697800000.123456) can0 100#b80b000000000000", &f, &ts));
    ASSERT_EQ(0xB8, f.data[0]);                         /* 소문자도 허용 */
    ASSERT_EQ(0x0B, f.data[1]);
    ASSERT_EQ(1697800000123456ull, ts);
}

static void test_ok_short_fraction(void) {
    /* 소수점 아래가 6자리보다 짧으면 뒤에 0을 채운 것으로 본다: 0.5 → 500000us */
    CanFrame f;
    uint64_t ts = 0;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.5) vcan0 200#01", &f, &ts));
    ASSERT_EQ(500000, ts);
    ASSERT_EQ(PARSE_OK, parse_candump_line("(2) vcan0 200#01", &f, &ts));   /* 소수점 없음 */
    ASSERT_EQ(2000000, ts);
}

static void test_ok_line_endings(void) {
    /* fgets로 읽으면 줄 끝에 '\n'(리눅스/맥) 또는 "\r\n"(윈도우)이 붙어 온다 */
    CanFrame f;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.1) vcan0 100#1027\n", &f, NULL));
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.1) vcan0 100#1027\r\n", &f, NULL));
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.1) vcan0 100#1027   ", &f, NULL));
    ASSERT_EQ(2, f.dlc);
}

static void test_ok_value_error_passes_parser(void) {
    /* 문법은 맞지만 ID > 0x7FF → 파서는 통과, can_frame_is_valid()에서 걸린다 (노트 17번 6번) */
    CanFrame f;
    ASSERT_EQ(PARSE_OK, parse_candump_line("(0.1) vcan0 800#00", &f, NULL));
    ASSERT_EQ(0x800, f.id);
    ASSERT_FALSE(can_frame_is_valid(&f));
}

/* ========== 건너뛰는 줄 (2번) ========== */

static void test_skip_lines(void) {
    CanFrame f;
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("", &f, NULL));
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("\n", &f, NULL));
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("\r\n", &f, NULL));
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("   \n", &f, NULL));
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("# 주석", &f, NULL));
    ASSERT_EQ(PARSE_SKIP, parse_candump_line("  # 들여쓴 주석", &f, NULL));
}

/* ========== 잘못된 예시 (4번) — 테이블 기반 ========== */

static void test_errors_table(void) {
    static const struct {
        const char *line;
        ParseResult expected;
    } cases[] = {
        { "vcan0 100#1027000000000000",               PARSE_ERR_TIMESTAMP },    /* 타임스탬프 없음 */
        { "0.100000 vcan0 100#1027000000000000",      PARSE_ERR_TIMESTAMP },    /* 괄호 없음 */
        { "(0.100000 vcan0 100#1027",                 PARSE_ERR_TIMESTAMP },    /* 닫는 괄호 없음 */
        { "(.5) vcan0 100#1027",                      PARSE_ERR_TIMESTAMP },    /* 정수 부분 없음 */
        { "(0.) vcan0 100#1027",                      PARSE_ERR_TIMESTAMP },    /* 점 뒤가 비어 있음 */
        { "(0.1234567) vcan0 100#1027",               PARSE_ERR_TIMESTAMP },    /* 소수점 7자리 */
        { "(0.100000) 100#1027000000000000",          PARSE_ERR_INTERFACE },    /* 인터페이스 없음 */
        { "(0.5)vcan0 200#01",                        PARSE_ERR_INTERFACE },    /* 공백 없음 */
        { "(0.100000) vcan0 1001027000000000000",     PARSE_ERR_ID },           /* '#' 없음 */
        { "(0.100000) vcan0 10#1027000000000000",     PARSE_ERR_ID },           /* ID 2자리 */
        { "(0.5) vcan0 0200#01",                      PARSE_ERR_ID },           /* ID 4자리 */
        { "(0.100000) vcan0 1G0#1027000000000000",    PARSE_ERR_ID },           /* 16진수 아님 */
        { "(0.100000) vcan0 100#10G7",                PARSE_ERR_DATA },         /* 데이터에 G */
        { "(0.5) vcan0 200#ZZ",                       PARSE_ERR_DATA },         /* 데이터에 Z */
        { "(0.100000) vcan0 100#102",                 PARSE_ERR_DATA },         /* 홀수 자리 */
        { "(0.5) vcan0 200#1",                        PARSE_ERR_DATA },         /* 1자리 */
        { "(0.100000) vcan0 100#102700000000000000",  PARSE_ERR_DATA },         /* 18자리 (9바이트) */
        { "(0.100000) vcan0 100#R",                   PARSE_ERR_UNSUPPORTED },  /* Remote Frame */
        { "(0.100000) vcan0 12345678#1027",           PARSE_ERR_UNSUPPORTED },  /* Extended ID */
        { "(0.100000) vcan0 100##01027",              PARSE_ERR_UNSUPPORTED },  /* CAN FD */
        { "(0.1) vcan0 100#1027 extra",               PARSE_ERR_TRAILING },     /* 뒤에 쓰레기 */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CanFrame f;
        ParseResult r = parse_candump_line(cases[i].line, &f, NULL);
        if (r != cases[i].expected) {
            printf("    → 줄: \"%s\" 기대 %s, 실제 %s\n", cases[i].line,
                   parse_result_name(cases[i].expected), parse_result_name(r));
        }
        ASSERT_EQ(cases[i].expected, r);
    }
}

/* ========== 실패하면 out을 건드리지 않음 ========== */

static void test_error_keeps_output(void) {
    CanFrame f = { .id = 0x123, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} };
    uint64_t ts = 777;
    ASSERT_EQ(PARSE_ERR_DATA, parse_candump_line("(0.1) vcan0 100#10G7", &f, &ts));
    ASSERT_EQ(0x123, f.id);                             /* 그대로 */
    ASSERT_EQ(3, f.dlc);
    ASSERT_EQ(0xAA, f.data[0]);
    ASSERT_EQ(777, ts);
}

static void test_null(void) {
    CanFrame f;
    ASSERT_EQ(PARSE_ERR_NULL, parse_candump_line(NULL, &f, NULL));
    ASSERT_EQ(PARSE_ERR_NULL, parse_candump_line("(0.1) vcan0 100#10", NULL, NULL));
}

static void test_result_names(void) {
    ASSERT_TRUE(strcmp("OK", parse_result_name(PARSE_OK)) == 0);
    ASSERT_TRUE(strcmp("ERR_DATA", parse_result_name(PARSE_ERR_DATA)) == 0);
    ASSERT_TRUE(strcmp("UNKNOWN", parse_result_name((ParseResult)99)) == 0);
}

int main(void) {
    printf("[test_log_parser]\n");
    RUN_TEST(test_ok_full_frame);
    RUN_TEST(test_ok_ign);
    RUN_TEST(test_ok_dlc_1);
    RUN_TEST(test_ok_dlc_0);
    RUN_TEST(test_ok_real_timestamp_lowercase);
    RUN_TEST(test_ok_short_fraction);
    RUN_TEST(test_ok_line_endings);
    RUN_TEST(test_ok_value_error_passes_parser);
    RUN_TEST(test_skip_lines);
    RUN_TEST(test_errors_table);
    RUN_TEST(test_error_keeps_output);
    RUN_TEST(test_null);
    RUN_TEST(test_result_names);
    return TEST_SUMMARY();
}
