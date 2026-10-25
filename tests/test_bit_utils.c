#include "bit_utils.h"
#include "test_framework.h"

static void test_bit_is_set(void) {
    ASSERT_TRUE(bit_is_set(0xB4u, 2));          /* 1011 0100 */
    ASSERT_FALSE(bit_is_set(0xB4u, 0));
    ASSERT_TRUE(bit_is_set(0x80000000u, 31));   /* 맨 위 비트 */
    ASSERT_FALSE(bit_is_set(0xFFFFFFFFu, 32));  /* 범위 밖 → false */
}

static void test_bit_set(void) {
    ASSERT_EQ(0x01u, bit_set(0x00u, 0));
    ASSERT_EQ(0xB5u, bit_set(0xB4u, 0));
    ASSERT_EQ(0xB4u, bit_set(0xB4u, 2));        /* 이미 1이면 그대로 */
    ASSERT_EQ(0xB4u, bit_set(0xB4u, 32));       /* 범위 밖 → 그대로 */
}

static void test_bit_clear(void) {
    ASSERT_EQ(0xB0u, bit_clear(0xB4u, 2));
    ASSERT_EQ(0xB4u, bit_clear(0xB4u, 0));      /* 이미 0이면 그대로 */
    ASSERT_EQ(0x7FFFFFFFu, bit_clear(0xFFFFFFFFu, 31));
    ASSERT_EQ(0xB4u, bit_clear(0xB4u, 40));     /* 범위 밖 → 그대로 */
}

static void test_bit_toggle(void) {
    ASSERT_EQ(0xB5u, bit_toggle(0xB4u, 0));
    ASSERT_EQ(0xB0u, bit_toggle(0xB4u, 2));
    ASSERT_EQ(0xB4u, bit_toggle(bit_toggle(0xB4u, 5), 5));   /* 두 번 뒤집으면 원래대로 */
    ASSERT_EQ(0xB4u, bit_toggle(0xB4u, 32));    /* 범위 밖 → 그대로 */
}

static void test_bit_count(void) {
    ASSERT_EQ(0, bit_count(0x00u));
    ASSERT_EQ(4, bit_count(0xB4u));
    ASSERT_EQ(32, bit_count(0xFFFFFFFFu));
}

/* M0-5 실험 3: 도어 상태 바이트 시나리오 */
static void test_door_status_scenario(void) {
    uint32_t doors = 0x00u;
    doors = bit_set(doors, 0);          /* 운전석 열림 */
    doors = bit_set(doors, 4);          /* 트렁크 열림 */
    ASSERT_EQ(0x11u, doors);
    doors = bit_clear(doors, 0);        /* 운전석 닫힘 */
    ASSERT_EQ(0x10u, doors);
    ASSERT_TRUE(bit_is_set(doors, 4));
    ASSERT_FALSE(bit_is_set(doors, 0));
}

int main(void) {
    printf("[test_bit_utils]\n");
    RUN_TEST(test_bit_is_set);
    RUN_TEST(test_bit_set);
    RUN_TEST(test_bit_clear);
    RUN_TEST(test_bit_toggle);
    RUN_TEST(test_bit_count);
    RUN_TEST(test_door_status_scenario);
    return TEST_SUMMARY();
}
