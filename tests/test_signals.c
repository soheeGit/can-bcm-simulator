#include <string.h>

#include "signals.h"
#include "test_framework.h"

#define ASSERT_STR_EQ(expected, actual) ASSERT_TRUE(strcmp((expected), (actual)) == 0)

/* 차속 프레임을 간단히 만드는 도우미 (data[0], data[1]만 채움) */
static CanFrame speed_frame(uint8_t b0, uint8_t b1) {
    CanFrame f = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 8, .data = {b0, b1} };
    return f;
}

/* ---------- 정상 값 ---------- */

static void test_speed_zero(void) {
    CanFrame f = speed_frame(0x00, 0x00);
    VehicleSpeed s;
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));
    ASSERT_TRUE(s.valid);
    ASSERT_EQ(0, s.centi_kmh);
}

static void test_speed_100kmh(void) {
    CanFrame f = speed_frame(0x10, 0x27);              /* 정의서 예시 1 */
    VehicleSpeed s;
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));
    ASSERT_TRUE(s.valid);
    ASSERT_EQ(10000, s.centi_kmh);
}

static void test_speed_max(void) {
    CanFrame f = speed_frame(0xFE, 0xFF);              /* raw 0xFFFE = 655.34 km/h (유효 최대) */
    VehicleSpeed s;
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));
    ASSERT_TRUE(s.valid);
    ASSERT_EQ(65534, s.centi_kmh);
}

static void test_speed_ignores_reserved_bytes(void) {
    /* data[2]~[7]은 예약. 값이 있어도 무시해야 한다 */
    CanFrame f = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 8,
                   .data = {0x10, 0x27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };
    VehicleSpeed s;
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));
    ASSERT_EQ(10000, s.centi_kmh);
}

/* ---------- 신호 무효 ---------- */

static void test_speed_invalid_value(void) {
    CanFrame f = speed_frame(0xFF, 0xFF);              /* 정의서 예시 2: 0xFFFF = 무효 */
    VehicleSpeed s = { .valid = true, .centi_kmh = 1234 };
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));         /* 프레임 자체는 해석 성공 */
    ASSERT_FALSE(s.valid);                             /* 하지만 신호는 "모름" */
}

/* ---------- 프레임을 해석할 수 없는 경우 → false ---------- */

static void test_speed_dlc_too_short(void) {
    CanFrame f = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 1, .data = {0x10} };
    VehicleSpeed s;
    ASSERT_FALSE(decode_vehicle_speed(&f, &s));
}

static void test_speed_dlc_min_ok(void) {
    CanFrame f = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 2, .data = {0x10, 0x27} };
    VehicleSpeed s;
    ASSERT_TRUE(decode_vehicle_speed(&f, &s));         /* DLC 8이 아니어도 최소 길이면 OK */
    ASSERT_EQ(10000, s.centi_kmh);
}

static void test_speed_wrong_id(void) {
    CanFrame f = { .id = MSG_ID_DOOR_STATUS, .dlc = 8, .data = {0x10, 0x27} };
    VehicleSpeed s;
    ASSERT_FALSE(decode_vehicle_speed(&f, &s));
}

static void test_speed_invalid_frame(void) {
    CanFrame f = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 9 };   /* DLC 9: 잘못된 프레임 */
    VehicleSpeed s;
    ASSERT_FALSE(decode_vehicle_speed(&f, &s));
}

static void test_speed_null(void) {
    CanFrame f = speed_frame(0x10, 0x27);
    VehicleSpeed s;
    ASSERT_FALSE(decode_vehicle_speed(NULL, &s));
    ASSERT_FALSE(decode_vehicle_speed(&f, NULL));
}

/* ---------- 문자열 출력 ---------- */

static void test_speed_format(void) {
    char buf[32];
    VehicleSpeed s100 = { .valid = true, .centi_kmh = 10000 };
    vehicle_speed_format(&s100, buf, sizeof(buf));
    ASSERT_STR_EQ("100.00 km/h", buf);

    VehicleSpeed slow = { .valid = true, .centi_kmh = 5 };     /* 0.05 km/h: 앞에 0 채우기 */
    vehicle_speed_format(&slow, buf, sizeof(buf));
    ASSERT_STR_EQ("0.05 km/h", buf);

    VehicleSpeed max = { .valid = true, .centi_kmh = 65534 };
    vehicle_speed_format(&max, buf, sizeof(buf));
    ASSERT_STR_EQ("655.34 km/h", buf);

    VehicleSpeed bad = { .valid = false };
    vehicle_speed_format(&bad, buf, sizeof(buf));
    ASSERT_STR_EQ("INVALID", buf);
}

int main(void) {
    printf("[test_signals]\n");
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_100kmh);
    RUN_TEST(test_speed_max);
    RUN_TEST(test_speed_ignores_reserved_bytes);
    RUN_TEST(test_speed_invalid_value);
    RUN_TEST(test_speed_dlc_too_short);
    RUN_TEST(test_speed_dlc_min_ok);
    RUN_TEST(test_speed_wrong_id);
    RUN_TEST(test_speed_invalid_frame);
    RUN_TEST(test_speed_null);
    RUN_TEST(test_speed_format);
    return TEST_SUMMARY();
}
