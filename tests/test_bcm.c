#include <string.h>

#include "bcm.h"
#include "test_framework.h"

/* M4-2: 헤더의 모양(타입, 초기값, 출력 규칙)만 확인한다. 전이 표 테스트는 M4-3, M4-7 */

static void test_state_values_match_0x400(void) {
    /* signal-spec.md 0x400 BcmState: 0 = IDLE, 1 = DRIVING_LOCKED, 2 = WARNING */
    ASSERT_EQ(0, BCM_STATE_IDLE);
    ASSERT_EQ(1, BCM_STATE_DRIVING_LOCKED);
    ASSERT_EQ(2, BCM_STATE_WARNING);
}

static void test_rule_numbers_match_table(void) {
    /* BCM_RULE_Tn의 값 = n (로그와 문서의 T 번호가 같게) */
    ASSERT_EQ(0, BCM_RULE_NONE);
    ASSERT_EQ(1, BCM_RULE_T1);
    ASSERT_EQ(3, BCM_RULE_T3);
    ASSERT_EQ(11, BCM_RULE_T11);
}

static void test_init(void) {
    BcmContext ctx;
    ctx.state = BCM_STATE_WARNING;   /* 쓰레기 값을 넣어 두고 */
    ctx.locked = true;
    bcm_init(&ctx);                  /* 초기화되는지 */
    ASSERT_EQ(BCM_STATE_IDLE, ctx.state);
    ASSERT_FALSE(ctx.locked);
}

static void test_outputs_follow_state(void) {
    /* 껍데기 bcm_step: 상태는 그대로, 출력은 상태에 맞게 채워진다 */
    VehicleInputs in;
    vehicle_inputs_init(&in);
    BcmContext ctx;
    bcm_init(&ctx);
    BcmOutputs out;

    bcm_step(&ctx, &in, 0u, &out);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_STATE_IDLE, out.prev_state);
    ASSERT_FALSE(out.state_changed);
    ASSERT_FALSE(out.warning_buzzer);
    ASSERT_FALSE(out.door_lock_cmd);
    ASSERT_EQ(BCM_RULE_NONE, out.rule);

    ctx.state = BCM_STATE_WARNING;   /* 테스트에서는 ctx를 직접 만들 수 있다 (전역 변수가 아니라서) */
    bcm_step(&ctx, &in, 0u, &out);
    ASSERT_TRUE(out.warning_buzzer); /* Moore: WARNING이면 경고음 */
}

static void test_null_args(void) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    BcmContext ctx;
    bcm_init(&ctx);
    BcmOutputs out;
    out.state = BCM_STATE_WARNING;   /* NULL이 있으면 out을 건드리지 않아야 한다 */

    bcm_init(NULL);                  /* 죽지 않기 */
    bcm_step(NULL, &in, 0u, &out);
    bcm_step(&ctx, NULL, 0u, &out);
    bcm_step(&ctx, &in, 0u, NULL);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);
}

static void test_names(void) {
    ASSERT_TRUE(strcmp("IDLE", bcm_state_name(BCM_STATE_IDLE)) == 0);
    ASSERT_TRUE(strcmp("DRIVING_LOCKED", bcm_state_name(BCM_STATE_DRIVING_LOCKED)) == 0);
    ASSERT_TRUE(strcmp("WARNING", bcm_state_name(BCM_STATE_WARNING)) == 0);
    ASSERT_TRUE(strcmp("UNKNOWN", bcm_state_name((BcmState)3)) == 0);   /* enum은 정수라 범위 밖 값이 올 수 있다 */
    ASSERT_TRUE(strcmp("-", bcm_rule_name(BCM_RULE_NONE)) == 0);
    ASSERT_TRUE(strcmp("T3", bcm_rule_name(BCM_RULE_T3)) == 0);
    ASSERT_TRUE(strcmp("T11", bcm_rule_name(BCM_RULE_T11)) == 0);
}

int main(void) {
    printf("[test_bcm]\n");
    RUN_TEST(test_state_values_match_0x400);
    RUN_TEST(test_rule_numbers_match_table);
    RUN_TEST(test_init);
    RUN_TEST(test_outputs_follow_state);
    RUN_TEST(test_null_args);
    RUN_TEST(test_names);
    return TEST_SUMMARY();
}
