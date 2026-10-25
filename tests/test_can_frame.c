#include <string.h>

#include "can_frame.h"
#include "test_framework.h"

/* 문자열 두 개가 같은지 비교 (strcmp는 같으면 0) */
#define ASSERT_STR_EQ(expected, actual) ASSERT_TRUE(strcmp((expected), (actual)) == 0)

/* ---------- can_frame_is_valid: DLC 경계값 ---------- */

static void test_valid_dlc_0(void) {
    CanFrame f = { .id = 0x100, .dlc = 0 };            /* 데이터 없는 프레임도 정상 */
    ASSERT_TRUE(can_frame_is_valid(&f));
}

static void test_valid_dlc_8(void) {
    CanFrame f = { .id = 0x100, .dlc = 8 };            /* 최대 길이 */
    ASSERT_TRUE(can_frame_is_valid(&f));
}

static void test_invalid_dlc_9(void) {
    CanFrame f = { .id = 0x100, .dlc = 9 };            /* 일반 CAN은 8바이트까지 */
    ASSERT_FALSE(can_frame_is_valid(&f));
}

static void test_invalid_dlc_15(void) {
    CanFrame f = { .id = 0x100, .dlc = 15 };           /* DLC 4비트 최댓값 */
    ASSERT_FALSE(can_frame_is_valid(&f));
}

/* ---------- can_frame_is_valid: ID 경계값 ---------- */

static void test_valid_id_0x000(void) {
    CanFrame f = { .id = 0x000, .dlc = 8 };
    ASSERT_TRUE(can_frame_is_valid(&f));
}

static void test_valid_id_0x7ff(void) {
    CanFrame f = { .id = 0x7FF, .dlc = 8 };            /* 11비트 최댓값 */
    ASSERT_TRUE(can_frame_is_valid(&f));
}

static void test_invalid_id_0x800(void) {
    CanFrame f = { .id = 0x800, .dlc = 8 };            /* 12비트 필요 → Standard 아님 */
    ASSERT_FALSE(can_frame_is_valid(&f));
}

static void test_invalid_null(void) {
    ASSERT_FALSE(can_frame_is_valid(NULL));
}

/* ---------- can_frame_format ---------- */

static void test_format_full(void) {
    CanFrame f = { .id = 0x100, .dlc = 8, .data = {0x10, 0x27} };   /* 나머지는 0 */
    char buf[CAN_FRAME_STR_SIZE];
    can_frame_format(&f, buf, sizeof(buf));
    ASSERT_STR_EQ("0x100 [8] 10 27 00 00 00 00 00 00", buf);
}

static void test_format_short_dlc(void) {
    CanFrame f = { .id = 0x200, .dlc = 1, .data = {0x11, 0xFF} };   /* dlc만큼만 출력 */
    char buf[CAN_FRAME_STR_SIZE];
    can_frame_format(&f, buf, sizeof(buf));
    ASSERT_STR_EQ("0x200 [1] 11", buf);
}

static void test_format_dlc_0(void) {
    CanFrame f = { .id = 0x7FF, .dlc = 0 };
    char buf[CAN_FRAME_STR_SIZE];
    can_frame_format(&f, buf, sizeof(buf));
    ASSERT_STR_EQ("0x7FF [0]", buf);
}

static void test_format_invalid_dlc_does_not_overflow(void) {
    /* DLC가 잘못돼도 data[8] 밖을 읽지 않고 8바이트만 출력해야 한다 */
    CanFrame f = { .id = 0x100, .dlc = 15, .data = {1, 2, 3, 4, 5, 6, 7, 8} };
    char buf[CAN_FRAME_STR_SIZE];
    can_frame_format(&f, buf, sizeof(buf));
    ASSERT_STR_EQ("0x100 [15] 01 02 03 04 05 06 07 08", buf);
}

static void test_format_small_buffer(void) {
    /* 버퍼가 작으면 넘치지 않고 잘려야 한다 */
    CanFrame f = { .id = 0x100, .dlc = 8 };
    char buf[6];
    can_frame_format(&f, buf, sizeof(buf));
    ASSERT_STR_EQ("0x100", buf);
}

int main(void) {
    printf("[test_can_frame]\n");
    RUN_TEST(test_valid_dlc_0);
    RUN_TEST(test_valid_dlc_8);
    RUN_TEST(test_invalid_dlc_9);
    RUN_TEST(test_invalid_dlc_15);
    RUN_TEST(test_valid_id_0x000);
    RUN_TEST(test_valid_id_0x7ff);
    RUN_TEST(test_invalid_id_0x800);
    RUN_TEST(test_invalid_null);
    RUN_TEST(test_format_full);
    RUN_TEST(test_format_short_dlc);
    RUN_TEST(test_format_dlc_0);
    RUN_TEST(test_format_invalid_dlc_does_not_overflow);
    RUN_TEST(test_format_small_buffer);
    return TEST_SUMMARY();
}
