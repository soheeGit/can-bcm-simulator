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

/* ========== 0x200 DOOR_STATUS (M2-4) ========== */

static CanFrame door_frame(uint8_t b0) {
    CanFrame f = { .id = MSG_ID_DOOR_STATUS, .dlc = 8, .data = {b0} };
    return f;
}

static void test_door_all_closed(void) {
    CanFrame f = door_frame(0x00);
    DoorStatus d;
    ASSERT_TRUE(decode_door_status(&f, &d));
    ASSERT_FALSE(d.fl);
    ASSERT_FALSE(d.fr);
    ASSERT_FALSE(d.rl);
    ASSERT_FALSE(d.rr);
    ASSERT_FALSE(d.trunk);
    ASSERT_FALSE(door_any_open(&d));
}

/* 하나만 열림 × 5가지: 표(table)로 정의하고 반복문으로 검사 (테이블 기반 테스트, M4-7 예고) */
static void test_door_one_open_each(void) {
    static const struct {
        uint8_t    data0;
        DoorStatus expected;
    } cases[] = {
        { 0x01, { .fl = true } },      /* bit 0 */
        { 0x02, { .fr = true } },      /* bit 1 */
        { 0x04, { .rl = true } },      /* bit 2 */
        { 0x08, { .rr = true } },      /* bit 3 */
        { 0x10, { .trunk = true } },   /* bit 4 */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CanFrame f = door_frame(cases[i].data0);
        DoorStatus d;
        ASSERT_TRUE(decode_door_status(&f, &d));
        ASSERT_EQ(cases[i].expected.fl,    d.fl);
        ASSERT_EQ(cases[i].expected.fr,    d.fr);
        ASSERT_EQ(cases[i].expected.rl,    d.rl);
        ASSERT_EQ(cases[i].expected.rr,    d.rr);
        ASSERT_EQ(cases[i].expected.trunk, d.trunk);
        ASSERT_TRUE(door_any_open(&d));
    }
}

static void test_door_all_open(void) {
    CanFrame f = door_frame(0x1F);                     /* 0001 1111 */
    DoorStatus d;
    ASSERT_TRUE(decode_door_status(&f, &d));
    ASSERT_TRUE(d.fl && d.fr && d.rl && d.rr && d.trunk);
}

static void test_door_spec_example(void) {
    CanFrame f = door_frame(0x11);                     /* 정의서 예시 3: FL + 트렁크 */
    DoorStatus d;
    ASSERT_TRUE(decode_door_status(&f, &d));
    ASSERT_TRUE(d.fl);
    ASSERT_FALSE(d.fr);
    ASSERT_TRUE(d.trunk);
}

static void test_door_ignores_reserved_bits(void) {
    /* bit 5~63은 예약. 1로 가득 차 있어도 도어는 전부 닫힘이어야 한다 */
    CanFrame f = { .id = MSG_ID_DOOR_STATUS, .dlc = 8,
                   .data = {0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };   /* 1110 0000 */
    DoorStatus d;
    ASSERT_TRUE(decode_door_status(&f, &d));
    ASSERT_FALSE(door_any_open(&d));
}

static void test_door_cannot_decode(void) {
    DoorStatus d;
    CanFrame dlc0  = { .id = MSG_ID_DOOR_STATUS, .dlc = 0 };
    CanFrame wrong = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 8, .data = {0x01} };
    CanFrame f = door_frame(0x01);
    ASSERT_FALSE(decode_door_status(&dlc0, &d));       /* DLC 0: data[0]이 없음 */
    ASSERT_FALSE(decode_door_status(&wrong, &d));      /* 다른 ID */
    ASSERT_FALSE(decode_door_status(NULL, &d));
    ASSERT_FALSE(decode_door_status(&f, NULL));
    ASSERT_FALSE(door_any_open(NULL));
}

static void test_door_format(void) {
    char buf[64];
    DoorStatus d = { .fl = true, .trunk = true };
    door_status_format(&d, buf, sizeof(buf));
    ASSERT_STR_EQ("FL=1 FR=0 RL=0 RR=0 TRUNK=1", buf);
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

    RUN_TEST(test_door_all_closed);
    RUN_TEST(test_door_one_open_each);
    RUN_TEST(test_door_all_open);
    RUN_TEST(test_door_spec_example);
    RUN_TEST(test_door_ignores_reserved_bits);
    RUN_TEST(test_door_cannot_decode);
    RUN_TEST(test_door_format);
    return TEST_SUMMARY();
}
