#include "dispatcher.h"
#include "test_framework.h"

static CanFrame frame(uint32_t id, uint8_t dlc, uint8_t b0, uint8_t b1) {
    CanFrame f = { .id = id, .dlc = dlc, .data = {b0, b1} };
    return f;
}

/* ---------- 초기 상태 ---------- */

static void test_init(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    ASSERT_FALSE(in.speed_received);
    ASSERT_FALSE(in.doors_received);
    ASSERT_FALSE(in.ign_received);
    ASSERT_FALSE(in.speed.valid);
    ASSERT_EQ(GEAR_INVALID, in.ign.gear);
}

/* ---------- 정상 프레임 → 해당 상태만 갱신 ---------- */

static void test_dispatch_speed(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_VEHICLE_SPEED, 8, 0x10, 0x27);

    ASSERT_EQ(DISPATCH_OK, can_dispatch(&f, &in, 1000, NULL));
    ASSERT_TRUE(in.speed_received);
    ASSERT_TRUE(in.speed.valid);
    ASSERT_EQ(10000, in.speed.centi_kmh);
    ASSERT_EQ(1000, in.speed_rx_ms);
    ASSERT_FALSE(in.doors_received);                    /* 다른 상태는 그대로 */
    ASSERT_FALSE(in.ign_received);
}

static void test_dispatch_door(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_DOOR_STATUS, 8, 0x11, 0x00);

    ASSERT_EQ(DISPATCH_OK, can_dispatch(&f, &in, 2000, NULL));
    ASSERT_TRUE(in.doors_received);
    ASSERT_TRUE(in.doors.fl);
    ASSERT_TRUE(in.doors.trunk);
    ASSERT_EQ(2000, in.doors_rx_ms);
    ASSERT_FALSE(in.speed_received);
}

static void test_dispatch_ign(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_IGN_STATUS, 8, 0x02, 0x03);

    ASSERT_EQ(DISPATCH_OK, can_dispatch(&f, &in, 3000, NULL));
    ASSERT_TRUE(in.ign_received);
    ASSERT_EQ(IGN_ON, in.ign.ign);
    ASSERT_EQ(GEAR_D, in.ign.gear);
    ASSERT_EQ(3000, in.ign_rx_ms);
}

/* ---------- 관심 없는 메시지 → 무시 ---------- */

static void test_dispatch_unknown_id_ignored(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(0x7FF, 8, 0xFF, 0xFF);

    ASSERT_EQ(DISPATCH_IGNORED, can_dispatch(&f, &in, 1000, NULL));
    ASSERT_FALSE(in.speed_received);
    ASSERT_FALSE(in.doors_received);
    ASSERT_FALSE(in.ign_received);
}

static void test_dispatch_own_bcm_status_ignored(void) {
    /* 우리가 보낸 0x400이 다시 들어와도(loopback) 입력으로 쓰지 않는다 */
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_BCM_STATUS, 8, 0x01, 0x01);
    ASSERT_EQ(DISPATCH_IGNORED, can_dispatch(&f, &in, 1000, NULL));
}

/* ---------- 해석 불가 → 오류, 이전 값 유지 ---------- */

static void test_dispatch_short_dlc_keeps_previous(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame good = frame(MSG_ID_VEHICLE_SPEED, 8, 0x10, 0x27);    /* 100.00 km/h */
    CanFrame bad  = frame(MSG_ID_VEHICLE_SPEED, 1, 0x00, 0x00);    /* DLC 1: 해석 불가 */

    ASSERT_EQ(DISPATCH_OK, can_dispatch(&good, &in, 1000, NULL));
    ASSERT_EQ(DISPATCH_ERROR, can_dispatch(&bad, &in, 1100, NULL));

    ASSERT_EQ(10000, in.speed.centi_kmh);              /* 깨진 프레임이 덮어쓰지 않았다 */
    ASSERT_EQ(1000, in.speed_rx_ms);                   /* 수신 시각도 그대로 (M4-6 타임아웃이 제대로 동작하려면) */
}

static void test_dispatch_invalid_frame(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_VEHICLE_SPEED, 9, 0x10, 0x27);        /* DLC 9 */
    ASSERT_EQ(DISPATCH_ERROR, can_dispatch(&f, &in, 1000, NULL));
    ASSERT_FALSE(in.speed_received);
}

static void test_dispatch_null(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_VEHICLE_SPEED, 8, 0x10, 0x27);
    ASSERT_EQ(DISPATCH_ERROR, can_dispatch(NULL, &in, 0, NULL));
    ASSERT_EQ(DISPATCH_ERROR, can_dispatch(&f, NULL, 0, NULL));
}

/* ---------- 신호 무효는 "정상 수신" ---------- */

static void test_dispatch_invalid_signal_is_ok(void) {
    /* 0xFFFF: 프레임은 정상, 신호가 "모름" → OK로 처리하고 speed.valid = false */
    VehicleInputs in;
    vehicle_inputs_init(&in);
    CanFrame f = frame(MSG_ID_VEHICLE_SPEED, 8, 0xFF, 0xFF);
    ASSERT_EQ(DISPATCH_OK, can_dispatch(&f, &in, 1000, NULL));
    ASSERT_TRUE(in.speed_received);
    ASSERT_FALSE(in.speed.valid);
}

/* ---------- 통계 ---------- */

static void test_dispatch_stats(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    DispatchStats stats = {0};

    CanFrame speed   = frame(MSG_ID_VEHICLE_SPEED, 8, 0x10, 0x27);
    CanFrame door    = frame(MSG_ID_DOOR_STATUS, 8, 0x00, 0x00);
    CanFrame unknown = frame(0x123, 8, 0x00, 0x00);
    CanFrame broken  = frame(MSG_ID_IGN_STATUS, 1, 0x02, 0x00);

    can_dispatch(&speed, &in, 0, &stats);
    can_dispatch(&door, &in, 0, &stats);
    can_dispatch(&unknown, &in, 0, &stats);
    can_dispatch(&broken, &in, 0, &stats);

    ASSERT_EQ(2, stats.ok);
    ASSERT_EQ(1, stats.ignored);
    ASSERT_EQ(1, stats.error);
}

/* ---------- 여러 프레임이 쌓여 차량 상태가 완성되는 시나리오 ---------- */

static void test_dispatch_scenario(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    static const struct { uint32_t t; uint32_t id; uint8_t b0; uint8_t b1; } seq[] = {
        {   0, MSG_ID_IGN_STATUS,    0x02, 0x00 },     /* 시동 ON, P */
        {  50, MSG_ID_DOOR_STATUS,   0x00, 0x00 },     /* 문 전부 닫힘 */
        { 100, MSG_ID_IGN_STATUS,    0x02, 0x03 },     /* D단 */
        { 150, 0x555,                0xAA, 0xBB },     /* 상관없는 메시지 */
        { 200, MSG_ID_VEHICLE_SPEED, 0xB8, 0x0B },     /* 30.00 km/h */
        { 300, MSG_ID_DOOR_STATUS,   0x01, 0x00 },     /* 운전석 열림! */
    };
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        CanFrame f = frame(seq[i].id, 8, seq[i].b0, seq[i].b1);
        can_dispatch(&f, &in, seq[i].t, NULL);
    }

    /* M4 상태 머신이 보게 될 최종 상태: "D단, 30 km/h로 달리는데 운전석 문이 열림" → WARNING 상황 */
    ASSERT_EQ(IGN_ON, in.ign.ign);
    ASSERT_EQ(GEAR_D, in.ign.gear);
    ASSERT_EQ(3000, in.speed.centi_kmh);
    ASSERT_TRUE(door_any_open(&in.doors));
    ASSERT_EQ(300, in.doors_rx_ms);
    ASSERT_EQ(200, in.speed_rx_ms);
}

int main(void) {
    printf("[test_dispatcher]\n");
    RUN_TEST(test_init);
    RUN_TEST(test_dispatch_speed);
    RUN_TEST(test_dispatch_door);
    RUN_TEST(test_dispatch_ign);
    RUN_TEST(test_dispatch_unknown_id_ignored);
    RUN_TEST(test_dispatch_own_bcm_status_ignored);
    RUN_TEST(test_dispatch_short_dlc_keeps_previous);
    RUN_TEST(test_dispatch_invalid_frame);
    RUN_TEST(test_dispatch_null);
    RUN_TEST(test_dispatch_invalid_signal_is_ok);
    RUN_TEST(test_dispatch_stats);
    RUN_TEST(test_dispatch_scenario);
    return TEST_SUMMARY();
}
