#include <stdio.h>
#include <string.h>

#include "log_reader.h"
#include "logger.h"
#include "test_framework.h"

/* 문자열을 임시 파일에 써서 "파일처럼" log_read_stream()에 넘긴다
 * (tmpfile(): 이름 없는 임시 파일. fclose하면 자동으로 지워진다)
 * fmemopen()은 맥에서 빈 문자열을 못 열어서 쓰지 않았다 */
static int read_text(const char *text, VehicleInputs *in, LogReadStats *stats) {
    FILE *f = tmpfile();
    if (f == NULL) {
        return -2;
    }
    fputs(text, f);
    rewind(f);                                     /* 쓴 다음 처음으로 되감기 */
    vehicle_inputs_init(in);
    int r = log_read_stream(f, in, stats);
    fclose(f);
    return r;
}

static void test_read_all_good(void) {
    const char *text =
        "(0.000000) vcan0 300#0203000000000000\n"
        "(0.100000) vcan0 100#1027000000000000\n"
        "(0.200000) vcan0 200#0100000000000000\n";
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(3, s.lines);
    ASSERT_EQ(3, s.frames);
    ASSERT_EQ(0, s.parse_errors);
    ASSERT_EQ(3, s.dispatch.ok);
    ASSERT_EQ(10000, in.speed.centi_kmh);
    ASSERT_TRUE(in.doors.fl);
    ASSERT_EQ(GEAR_D, in.ign.gear);
}

static void test_read_mixed_continues_after_errors(void) {
    /* 잘못된 줄이 있어도 멈추지 않고 끝까지 읽는다 */
    const char *text =
        "(0.0) vcan0 100#B80B000000000000\n"     /* OK: 30 km/h */
        "(0.1) vcan0 100#10G7\n"                 /* 형식 오류 */
        "garbage\n"                              /* 형식 오류 */
        "(0.2) vcan0 100#1027000000000000\n";    /* OK: 100 km/h (마지막 값이 남아야 함) */
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(4, s.lines);
    ASSERT_EQ(2, s.frames);
    ASSERT_EQ(2, s.parse_errors);
    ASSERT_EQ(10000, in.speed.centi_kmh);
}

static void test_read_skips_blank_and_comments(void) {
    const char *text =
        "# 주석\n"
        "\n"
        "(0.0) vcan0 100#1027000000000000\n"
        "   \n";
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(4, s.lines);
    ASSERT_EQ(3, s.skipped);
    ASSERT_EQ(1, s.frames);
    ASSERT_EQ(0, s.parse_errors);
}

static void test_read_relative_time(void) {
    /* 실제 candump 시각(큰 숫자)도 첫 프레임 기준 상대 ms로 바뀌어야 한다 */
    const char *text =
        "(1697800000.000000) can0 300#0203000000000000\n"
        "(1697800000.250000) can0 100#1027000000000000\n";
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(0, in.ign_rx_ms);
    ASSERT_EQ(250, in.speed_rx_ms);
}

static void test_read_too_long_line(void) {
    /* 버퍼(128)보다 긴 줄: 그 줄만 오류로 세고, 나머지 글자가 다음 줄로 섞이면 안 된다 */
    char text[512];
    char long_part[201];
    memset(long_part, '1', 200);
    long_part[200] = '\0';
    snprintf(text, sizeof(text),
             "(0.0) vcan0 %s\n"
             "(0.1) vcan0 100#1027000000000000\n", long_part);
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(2, s.lines);                       /* 긴 줄이 여러 줄로 쪼개져 세어지면 안 됨 */
    ASSERT_EQ(1, s.parse_errors);
    ASSERT_EQ(1, s.frames);
    ASSERT_EQ(10000, in.speed.centi_kmh);
}

static void test_read_last_line_without_newline(void) {
    /* 파일 마지막 줄에 '\n'이 없어도 읽혀야 한다 */
    const char *text = "(0.0) vcan0 100#1027000000000000";
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(1, s.frames);
    ASSERT_EQ(10000, in.speed.centi_kmh);
}

static void test_read_empty(void) {
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text("", &in, &s));
    ASSERT_EQ(0, s.lines);
    ASSERT_FALSE(in.speed_received);
}

static void test_read_dispatch_stats(void) {
    /* 파싱은 됐지만 디스패처에서 무시/오류인 경우 */
    const char *text =
        "(0.0) vcan0 555#AA\n"                    /* IGNORED */
        "(0.1) vcan0 100#10\n"                    /* 파싱 OK, DLC 1 → 디스패처 ERROR */
        "(0.2) vcan0 200#01\n";                   /* OK */
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(0, read_text(text, &in, &s));
    ASSERT_EQ(3, s.frames);
    ASSERT_EQ(1, s.dispatch.ok);
    ASSERT_EQ(1, s.dispatch.ignored);
    ASSERT_EQ(1, s.dispatch.error);
}

static void test_file_not_found(void) {
    VehicleInputs in;
    LogReadStats s;
    vehicle_inputs_init(&in);
    ASSERT_EQ(-1, log_read_file("no_such_file.log", &in, &s));
}

static void test_file_example(void) {
    /* 저장소의 예시 파일 (make test는 저장소 루트에서 실행된다) */
    VehicleInputs in;
    LogReadStats s;
    vehicle_inputs_init(&in);
    ASSERT_EQ(0, log_read_file("data/examples/mixed.log", &in, &s));
    ASSERT_EQ(16, s.lines);
    ASSERT_EQ(4, s.parse_errors);                /* 10G7, 타임스탬프 없음, R, 너무 긴 줄 */
    ASSERT_EQ(7, s.frames);
    ASSERT_EQ(10000, in.speed.centi_kmh);
}

static void test_null_args(void) {
    VehicleInputs in;
    LogReadStats s;
    ASSERT_EQ(-1, log_read_stream(NULL, &in, &s));
    ASSERT_EQ(-1, log_read_file(NULL, &in, &s));
}

int main(void) {
    printf("[test_log_reader]\n");
    log_set_level(LOG_LEVEL_OFF);                  /* 테스트 중에는 로그를 안 찍는다 */
    RUN_TEST(test_read_all_good);
    RUN_TEST(test_read_mixed_continues_after_errors);
    RUN_TEST(test_read_skips_blank_and_comments);
    RUN_TEST(test_read_relative_time);
    RUN_TEST(test_read_too_long_line);
    RUN_TEST(test_read_last_line_without_newline);
    RUN_TEST(test_read_empty);
    RUN_TEST(test_read_dispatch_stats);
    RUN_TEST(test_file_not_found);
    RUN_TEST(test_file_example);
    RUN_TEST(test_null_args);
    return TEST_SUMMARY();
}
