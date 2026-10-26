#include "can_signal.h"
#include "test_framework.h"

/* ---------- 1비트 신호 ---------- */

static void test_get_1bit(void) {
    /* 정의서 예시 3: 200#11... → 운전석(bit 0) + 트렁크(bit 4) 열림 */
    const uint8_t data[8] = {0x11};
    ASSERT_EQ(1, can_get_bits(data, 0, 1));    /* DoorFL */
    ASSERT_EQ(0, can_get_bits(data, 1, 1));    /* DoorFR */
    ASSERT_EQ(1, can_get_bits(data, 4, 1));    /* Trunk */
}

/* ---------- 바이트 안의 여러 비트 ---------- */

static void test_get_bits_inside_byte(void) {
    /* 정의서 예시 4: 300#0203... → IgnState 2(ON), Gear 3(D) */
    const uint8_t data[8] = {0x02, 0x03};
    ASSERT_EQ(2, can_get_bits(data, 0, 2));    /* IgnState */
    ASSERT_EQ(3, can_get_bits(data, 8, 3));    /* Gear */
}

static void test_get_ignores_neighbor_bits(void) {
    /* 0xE3 = 1110 0011 → 비트 2~4는 전부 0. 양옆의 1이 섞이면 안 된다 */
    const uint8_t data[8] = {0xE3};
    ASSERT_EQ(0, can_get_bits(data, 2, 3));
}

/* ---------- 바이트 단위 신호 ---------- */

static void test_get_8bit_aligned(void) {
    const uint8_t data[8] = {0x00, 0xAB};
    ASSERT_EQ(0xAB, can_get_bits(data, 8, 8));
}

static void test_get_16bit_little_endian(void) {
    /* 정의서 예시 1: 100#1027... → 0x2710 = 10000 (100.00 km/h) */
    const uint8_t speed_100[8] = {0x10, 0x27};
    ASSERT_EQ(10000, can_get_bits(speed_100, 0, 16));

    /* M1-3 연습 문제 A, B */
    const uint8_t speed_30[8] = {0xB8, 0x0B};
    ASSERT_EQ(3000, can_get_bits(speed_30, 0, 16));
    const uint8_t speed_300[8] = {0x30, 0x75};
    ASSERT_EQ(30000, can_get_bits(speed_300, 0, 16));
}

/* ---------- 바이트 경계를 넘는 신호 ---------- */

static void test_get_cross_byte_8bit(void) {
    /* M1-4 실험 2 [3]: 비트필드는 0x0B로 틀렸던 그 경우 → 0xBA가 나와야 한다 */
    const uint8_t data[8] = {0xA0, 0x0B};
    ASSERT_EQ(0xBA, can_get_bits(data, 4, 8));
}

static void test_get_cross_byte_12bit(void) {
    /* data[0] = 0x30, data[1] = 0x12 → 합치면 0x1230 → >> 4 → 0x123 */
    const uint8_t data[8] = {0x30, 0x12};
    ASSERT_EQ(0x123, can_get_bits(data, 4, 12));
}

/* ---------- 끝자리 / 최대 길이 ---------- */

static void test_get_last_bit_63(void) {
    const uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0x80};
    ASSERT_EQ(1, can_get_bits(data, 63, 1));
}

static void test_get_alive_counter(void) {
    /* 정의서 0x400 AliveCounter: start 56, 길이 4 → data[7]의 아래 4비트 */
    const uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0xFC};   /* 위 4비트 F는 예약 */
    ASSERT_EQ(12, can_get_bits(data, 56, 4));
}

static void test_get_32bit_full(void) {
    /* 길이 32: 마스크 계산에서 (1u << 32) UB를 피했는지 확인 */
    const uint8_t data[8] = {0x78, 0x56, 0x34, 0x12};
    ASSERT_EQ(0x12345678u, can_get_bits(data, 0, 32));
}

static void test_get_32bit_upper_half(void) {
    const uint8_t data[8] = {0, 0, 0, 0, 0xAA, 0xBB, 0xCC, 0xDD};
    ASSERT_EQ(0xDDCCBBAAu, can_get_bits(data, 32, 32));
}

/* ---------- 잘못된 인자 → 0 ---------- */

static void test_get_invalid_length_0(void) {
    const uint8_t data[8] = {0xFF};
    ASSERT_EQ(0, can_get_bits(data, 0, 0));
}

static void test_get_invalid_length_33(void) {
    const uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(0, can_get_bits(data, 0, 33));
}

static void test_get_invalid_out_of_frame(void) {
    /* 60 + 8 = 68 > 64 → 8바이트 밖 */
    const uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(0, can_get_bits(data, 60, 8));
}

static void test_get_invalid_null(void) {
    ASSERT_EQ(0, can_get_bits(NULL, 0, 8));
}

int main(void) {
    printf("[test_can_signal]\n");
    RUN_TEST(test_get_1bit);
    RUN_TEST(test_get_bits_inside_byte);
    RUN_TEST(test_get_ignores_neighbor_bits);
    RUN_TEST(test_get_8bit_aligned);
    RUN_TEST(test_get_16bit_little_endian);
    RUN_TEST(test_get_cross_byte_8bit);
    RUN_TEST(test_get_cross_byte_12bit);
    RUN_TEST(test_get_last_bit_63);
    RUN_TEST(test_get_alive_counter);
    RUN_TEST(test_get_32bit_full);
    RUN_TEST(test_get_32bit_upper_half);
    RUN_TEST(test_get_invalid_length_0);
    RUN_TEST(test_get_invalid_length_33);
    RUN_TEST(test_get_invalid_out_of_frame);
    RUN_TEST(test_get_invalid_null);
    return TEST_SUMMARY();
}
