#include <string.h>

#include "bcm.h"
#include "test_framework.h"

/* 전이 표(docs/fsm-design.md 4번)의 규칙마다 테스트한다.
 * 모든 줄을 표 하나로 정리하는 건 M4-7 (table-driven) */

#define NOW 10000u   /* 테스트 시각: 10초 */

/* ---------- 테스트용 입력 만들기 ---------- */

/* "시동 ON, 기어 D, 문 전부 닫힘, 차속 centi"를 방금(NOW) 받은 입력. 필요한 곳만 고쳐서 쓴다 */
static VehicleInputs driving(uint16_t centi) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    in.ign.ign = IGN_ON;
    in.ign.gear = GEAR_D;
    in.speed.valid = true;
    in.speed.centi_kmh = centi;
    in.ign_received = true;
    in.speed_received = true;
    in.doors_received = true;
    in.ign_rx_ms = NOW;
    in.speed_rx_ms = NOW;
    in.doors_rx_ms = NOW;
    in.doors_rx_count = 1u;          /* 도어 프레임 1개를 받은 상태 (디바운싱의 첫 샘플 = 바로 인정) */
    return in;
}

/* 새 도어 프레임이 하나 더 온 것처럼 (디바운싱은 도어 프레임 수를 센다, M4-5) */
static void new_door_frame(VehicleInputs *in, bool fl_open) {
    in->doors.fl = fl_open;
    in->doors_rx_count++;
}

/* 원하는 상태에서 시작해서 판단 한 번 (given: ctx, when: bcm_step) */
static BcmOutputs step_from(BcmContext *ctx, BcmState state, bool locked, const VehicleInputs *in) {
    bcm_init(ctx);                   /* 디바운싱 기억까지 깨끗하게 (M4-5) */
    ctx->state = state;
    ctx->locked = locked;
    BcmOutputs out;
    bcm_step(ctx, in, NOW, &out);
    return out;
}

/* ---------- 모양 (M4-2) ---------- */

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
    ASSERT_TRUE(strcmp("RESET", bcm_rule_name(BCM_RULE_RESET)) == 0);
    ASSERT_TRUE(strcmp("정차, P단", bcm_rule_reason(BCM_RULE_T7)) == 0);
}

/* ---------- T1: 모든 상태 ---------- */

static void test_t1_ignition_off(void) {
    BcmContext ctx;
    VehicleInputs in = driving(5000u);
    in.ign.ign = IGN_OFF;

    BcmOutputs out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T1, out.rule);
    ASSERT_FALSE(ctx.locked);                          /* IDLE로 가면 잠금 기억을 지운다 */

    out = step_from(&ctx, BCM_STATE_WARNING, false, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_FALSE(out.warning_buzzer);

    in.ign.ign = IGN_ACC;                              /* ACC도 시동 꺼짐으로 본다 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_RULE_T1, out.rule);
}

/* ---------- IDLE: T2 ~ T4 ---------- */

static void test_t2_idle_waits_without_signals(void) {
    BcmContext ctx;
    VehicleInputs in;
    vehicle_inputs_init(&in);                          /* 시동 직후: 아무 신호도 안 옴 */
    BcmOutputs out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T2, out.rule);
    ASSERT_FALSE(out.warning_buzzer);                  /* 경고음 울리지 않음 */

    in = driving(5000u);
    in.speed.valid = false;                            /* 차속 0xFFFF → 모르는 채로 잠그지 않음 */
    out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_RULE_T2, out.rule);
    ASSERT_FALSE(out.door_lock_cmd);
}

static void test_t3_auto_lock(void) {
    BcmContext ctx;
    VehicleInputs in = driving(1600u);
    BcmOutputs out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
    ASSERT_EQ(BCM_RULE_T3, out.rule);
    ASSERT_TRUE(out.door_lock_cmd);
    ASSERT_TRUE(out.state_changed);
    ASSERT_EQ(BCM_STATE_IDLE, out.prev_state);
    ASSERT_TRUE(ctx.locked);

    in = driving(1500u);                               /* 경계: 15.00은 "이상"이라 잠금 */
    out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_RULE_T3, out.rule);

    in = driving(1499u);                               /* 14.99는 아직 */
    out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_NONE, out.rule);
}

static void test_t4_idle_door_open_moving(void) {
    BcmContext ctx;
    VehicleInputs in = driving(1600u);
    in.doors.fl = true;
    BcmOutputs out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);
    ASSERT_EQ(BCM_RULE_T4, out.rule);
    ASSERT_TRUE(out.warning_buzzer);
    ASSERT_FALSE(out.door_lock_cmd);                   /* 문이 열려 있으니 잠그지 않는다 */
    ASSERT_FALSE(ctx.locked);

    in.speed.centi_kmh = 300u;                         /* 경계: 3.00은 "초과"가 아님 */
    out = step_from(&ctx, BCM_STATE_IDLE, false, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
}

/* ---------- DRIVING_LOCKED: T5 ~ T7 ---------- */

static void test_t5_fail_safe(void) {
    BcmContext ctx;
    VehicleInputs in = driving(5000u);
    in.speed_rx_ms = NOW - 600u;                       /* 차속 600ms째 안 옴 (시나리오 03) */
    BcmOutputs out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);
    ASSERT_EQ(BCM_RULE_T5, out.rule);
    ASSERT_TRUE(out.warning_buzzer);
    ASSERT_TRUE(ctx.locked);                           /* 주행 중이라 잠금 기억은 유지 */

    in.speed_rx_ms = NOW - 500u;                       /* 경계: 딱 500ms는 아직 끊김 아님 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);

    in = driving(5000u);
    in.speed.valid = false;                            /* 차속 0xFFFF (시나리오 04) */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_RULE_T5, out.rule);

    in = driving(5000u);
    in.doors_rx_ms = NOW - 1000u;                      /* 도어 신호 끊김 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_RULE_T5, out.rule);

    in = driving(5000u);
    in.ign_received = false;                           /* 시동 신호를 모름 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_RULE_T5, out.rule);
}

static void test_t6_door_open_while_driving(void) {
    BcmContext ctx;
    VehicleInputs in = driving(4000u);
    in.doors.trunk = true;                             /* 트렁크도 문 */
    BcmOutputs out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);
    ASSERT_EQ(BCM_RULE_T6, out.rule);
    ASSERT_TRUE(out.warning_buzzer);

    in.speed.centi_kmh = 200u;                         /* 2 km/h: 달리는 중이 아님 → 유지 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
    ASSERT_EQ(BCM_RULE_NONE, out.rule);
}

static void test_t7_parked(void) {
    BcmContext ctx;
    VehicleInputs in = driving(0u);
    in.ign.gear = GEAR_P;
    BcmOutputs out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T7, out.rule);
    ASSERT_FALSE(ctx.locked);                          /* 주행 끝: 잠금 기억 지움 */

    in.ign.gear = GEAR_D;                              /* 0 km/h라도 D면 신호 대기 → 유지 */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);

    in.ign.gear = GEAR_INVALID;                        /* 기어를 모르면 돌아가지 않을 뿐 (Fail-safe 아님) */
    out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
    ASSERT_EQ(BCM_RULE_NONE, out.rule);
}

/* ---------- WARNING: T8 ~ T11 ---------- */

static void test_t8_warning_holds(void) {
    BcmContext ctx;
    VehicleInputs in = driving(5000u);
    in.speed_received = false;
    BcmOutputs out = step_from(&ctx, BCM_STATE_WARNING, true, &in);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);
    ASSERT_EQ(BCM_RULE_T8, out.rule);
    ASSERT_TRUE(out.warning_buzzer);
    ASSERT_FALSE(out.state_changed);
}

static void test_t9_back_to_locked(void) {
    BcmContext ctx;
    VehicleInputs in = driving(4000u);                 /* 문 전부 닫힘 */
    BcmOutputs out = step_from(&ctx, BCM_STATE_WARNING, true, &in);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
    ASSERT_EQ(BCM_RULE_T9, out.rule);
    ASSERT_FALSE(out.door_lock_cmd);                   /* 이미 잠겨 있으니 다시 안 보냄 */
    ASSERT_FALSE(out.warning_buzzer);
}

static void test_t10_back_to_idle(void) {
    BcmContext ctx;
    VehicleInputs in = driving(2000u);
    BcmOutputs out = step_from(&ctx, BCM_STATE_WARNING, false, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T10, out.rule);

    out = step_from(&ctx, ctx.state, ctx.locked, &in);   /* 다음 판단: IDLE + 20 km/h + 문 닫힘 → T3 */
    ASSERT_EQ(BCM_RULE_T3, out.rule);
    ASSERT_TRUE(out.door_lock_cmd);
}

static void test_t11_stopped(void) {
    BcmContext ctx;
    VehicleInputs in = driving(0u);
    in.doors.fl = true;
    BcmOutputs out = step_from(&ctx, BCM_STATE_WARNING, true, &in);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T11, out.rule);
    ASSERT_FALSE(out.warning_buzzer);
    ASSERT_FALSE(ctx.locked);
}

/* ---------- 우선순위, default, 1회 출력 ---------- */

static void test_priority(void) {
    BcmContext ctx;

    /* 문 열림 + 차속 끊김: T5가 T6보다 위 */
    VehicleInputs in = driving(5000u);
    in.doors.fl = true;
    in.speed_rx_ms = NOW - 600u;
    BcmOutputs out = step_from(&ctx, BCM_STATE_DRIVING_LOCKED, true, &in);
    ASSERT_EQ(BCM_RULE_T5, out.rule);

    /* 문 닫힘 + 차속 끊김: T8이 T9보다 위 (왔다 갔다 하지 않게) */
    in = driving(5000u);
    in.speed_rx_ms = NOW - 600u;
    out = step_from(&ctx, BCM_STATE_WARNING, true, &in);
    ASSERT_EQ(BCM_RULE_T8, out.rule);

    /* 시동 OFF + 차속 끊김: T1이 T8보다 위 */
    in.ign.ign = IGN_OFF;
    out = step_from(&ctx, BCM_STATE_WARNING, true, &in);
    ASSERT_EQ(BCM_RULE_T1, out.rule);
}

static void test_unknown_state_resets(void) {
    BcmContext ctx;
    VehicleInputs in = driving(5000u);
    BcmOutputs out = step_from(&ctx, (BcmState)7, true, &in);   /* 메모리가 깨졌다고 치고 */
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_RESET, out.rule);
    ASSERT_FALSE(ctx.locked);
}

static void test_lock_cmd_only_once(void) {
    BcmContext ctx;
    bcm_init(&ctx);
    VehicleInputs in = driving(1600u);
    BcmOutputs out;

    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_TRUE(out.door_lock_cmd);                    /* 들어가는 순간 */
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_FALSE(out.door_lock_cmd);                   /* 그다음부터는 안 냄 (Mealy) */
    ASSERT_FALSE(out.state_changed);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_FALSE(out.door_lock_cmd);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
}

/* ---------- 히스테리시스 (M4-4) ---------- */

/* 같은 BCM으로 판단을 여러 번: 차속만 바꿔 가며 count번 돌리고, 잠금 명령/상태 전이 횟수를 센다 */
typedef struct {
    uint32_t lock_cmds;
    uint32_t transitions;
    uint32_t buzzer_off_while_warning;   /* WARNING에 들어간 뒤 경고음이 꺼진 횟수 (깜빡임) */
} JitterResult;

static JitterResult run_jitter(BcmContext *ctx, VehicleInputs *in, uint16_t low, uint16_t high, uint32_t count) {
    JitterResult r = {0u, 0u, 0u};
    bool entered_warning = false;
    for (uint32_t i = 0u; i < count; i++) {
        in->speed.centi_kmh = (i % 2u == 0u) ? low : high;     /* low, high, low, high, ... */
        in->doors_rx_count++;                                   /* 매번 새 프레임이 온 것처럼 */
        BcmOutputs out;
        bcm_step(ctx, in, NOW, &out);
        if (out.door_lock_cmd) { r.lock_cmds++; }
        if (out.state_changed) { r.transitions++; }
        if (out.state == BCM_STATE_WARNING) { entered_warning = true; }
        if (entered_warning && !out.warning_buzzer) { r.buzzer_off_while_warning++; }
    }
    return r;
}

static void test_hysteresis_lock_jitter(void) {
    /* 막히는 길: 14.90 ↔ 15.10 km/h를 30번 오간다 (시나리오 05) */
    BcmContext ctx;
    bcm_init(&ctx);
    VehicleInputs in = driving(0u);
    JitterResult r = run_jitter(&ctx, &in, 1490u, 1510u, 30u);
    ASSERT_EQ(1, r.lock_cmds);                         /* 잠금 명령은 처음 15.10에서 딱 한 번 */
    ASSERT_EQ(1, r.transitions);                       /* IDLE → DRIVING_LOCKED 한 번뿐. 14.90에서 풀리지 않음 */
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, ctx.state);

    /* 경계에 딱 붙어서: 14.99 ↔ 15.00 */
    bcm_init(&ctx);
    r = run_jitter(&ctx, &in, 1499u, 1500u, 30u);
    ASSERT_EQ(1, r.lock_cmds);
    ASSERT_EQ(1, r.transitions);
}

static void test_hysteresis_unlock_only_when_parked(void) {
    /* 잠금 상태에서 0이 아닌 아주 느린 속도(0.01 ~ 14.99)를 오가도 풀리지 않는다. 풀리는 건 0 + P뿐 */
    BcmContext ctx;
    bcm_init(&ctx);
    VehicleInputs in = driving(1600u);
    BcmOutputs out;
    bcm_step(&ctx, &in, NOW, &out);                    /* 잠금 */
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);

    JitterResult r = run_jitter(&ctx, &in, 1u, 1499u, 30u);
    ASSERT_EQ(0, r.transitions);
    ASSERT_EQ(0, r.lock_cmds);

    in.ign.gear = GEAR_P;                              /* 0.01 km/h에서 P여도 아직 (완전히 멈춰야) */
    in.speed.centi_kmh = 1u;
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);

    in.speed.centi_kmh = 0u;                           /* 0 + P → 해제 */
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_IDLE, out.state);
    ASSERT_EQ(BCM_RULE_T7, out.rule);
}

static void test_hysteresis_warning_jitter(void) {
    /* 문이 열린 채 2 ↔ 4 km/h를 오간다 (주차장에서 문 열고 천천히): 경고음이 깜빡이면 안 된다 */
    BcmContext ctx;
    bcm_init(&ctx);
    VehicleInputs in = driving(1600u);
    BcmOutputs out;
    bcm_step(&ctx, &in, NOW, &out);                    /* 먼저 잠금 주행 */
    in.doors.fl = true;

    JitterResult r = run_jitter(&ctx, &in, 200u, 400u, 30u);
    ASSERT_EQ(1, r.transitions);                       /* 첫 4 km/h에서 WARNING으로 한 번 */
    ASSERT_EQ(0, r.buzzer_off_while_warning);          /* 2 km/h로 떨어져도 경고음 유지 */
    ASSERT_EQ(BCM_STATE_WARNING, ctx.state);

    in.speed.centi_kmh = 0u;                           /* 완전히 멈추면 그때 끝 (T11) */
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_RULE_T11, out.rule);
    ASSERT_FALSE(out.warning_buzzer);
}

/* ---------- 디바운싱 (M4-5) ---------- */

/* 잠금 주행(40 km/h, 문 닫힘)까지 만들어 두기 */
static void start_locked_driving(BcmContext *ctx, VehicleInputs *in) {
    bcm_init(ctx);
    *in = driving(4000u);
    BcmOutputs out;
    bcm_step(ctx, in, NOW, &out);                      /* 첫 샘플(닫힘) 바로 인정 + T3 잠금 */
    ctx->state = BCM_STATE_DRIVING_LOCKED;
}

static void test_debounce_first_sample_immediate(void) {
    /* 첫 도어 프레임은 기다리지 않는다: 문 열고 출발하면 바로 T4 */
    BcmContext ctx;
    bcm_init(&ctx);
    VehicleInputs in = driving(1600u);
    in.doors.fl = true;
    BcmOutputs out;
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_RULE_T4, out.rule);
}

static void test_debounce_noise_ignored(void) {
    BcmContext ctx;
    VehicleInputs in;
    BcmOutputs out;
    start_locked_driving(&ctx, &in);

    new_door_frame(&in, true);                         /* 노이즈 1번 */
    bcm_step(&ctx, &in, NOW, &out);
    new_door_frame(&in, false);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 무시 */

    new_door_frame(&in, true);                         /* 노이즈 2번 연속 */
    bcm_step(&ctx, &in, NOW, &out);
    new_door_frame(&in, true);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 아직 무시 (2 < 3) */
    new_door_frame(&in, false);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
}

static void test_debounce_third_frame_accepted(void) {
    BcmContext ctx;
    VehicleInputs in;
    BcmOutputs out;
    start_locked_driving(&ctx, &in);

    new_door_frame(&in, true);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 1번째 */
    new_door_frame(&in, true);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 2번째 */
    new_door_frame(&in, true);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);           /* 3번째에 인정 → T6 */
    ASSERT_EQ(BCM_RULE_T6, out.rule);

    /* 닫힐 때도 3번 연속이어야 */
    new_door_frame(&in, false);
    bcm_step(&ctx, &in, NOW, &out);
    new_door_frame(&in, false);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_WARNING, out.state);           /* 2번째까지는 경고 유지 */
    new_door_frame(&in, false);
    bcm_step(&ctx, &in, NOW, &out);
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 3번째에 T9 */
    ASSERT_EQ(BCM_RULE_T9, out.rule);
}

static void test_debounce_counts_frames_not_steps(void) {
    /* 도어 프레임 하나에 판단이 여러 번(0x100, 0x300 때문에) 있어도 1번으로 센다 */
    BcmContext ctx;
    VehicleInputs in;
    BcmOutputs out;
    start_locked_driving(&ctx, &in);

    new_door_frame(&in, true);                         /* 열림 프레임은 딱 1개 */
    for (int i = 0; i < 10; i++) {
        bcm_step(&ctx, &in, NOW, &out);                /* 판단은 10번 */
    }
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);    /* 그래도 1번으로 쳐서 무시 */
}

static void test_debounce_doors_independent(void) {
    /* 문마다 따로 센다: FL과 RR이 번갈아 한 번씩 튀면 둘 다 3번이 안 됨 */
    BcmContext ctx;
    VehicleInputs in;
    BcmOutputs out;
    start_locked_driving(&ctx, &in);

    for (int i = 0; i < 6; i++) {
        in.doors.fl = (i % 2 == 0);
        in.doors.rr = (i % 2 == 1);
        in.doors_rx_count++;
        bcm_step(&ctx, &in, NOW, &out);
        ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, out.state);
    }
}

int main(void) {
    printf("[test_bcm]\n");
    RUN_TEST(test_state_values_match_0x400);
    RUN_TEST(test_rule_numbers_match_table);
    RUN_TEST(test_init);
    RUN_TEST(test_null_args);
    RUN_TEST(test_names);
    RUN_TEST(test_t1_ignition_off);
    RUN_TEST(test_t2_idle_waits_without_signals);
    RUN_TEST(test_t3_auto_lock);
    RUN_TEST(test_t4_idle_door_open_moving);
    RUN_TEST(test_t5_fail_safe);
    RUN_TEST(test_t6_door_open_while_driving);
    RUN_TEST(test_t7_parked);
    RUN_TEST(test_t8_warning_holds);
    RUN_TEST(test_t9_back_to_locked);
    RUN_TEST(test_t10_back_to_idle);
    RUN_TEST(test_t11_stopped);
    RUN_TEST(test_priority);
    RUN_TEST(test_unknown_state_resets);
    RUN_TEST(test_lock_cmd_only_once);
    RUN_TEST(test_hysteresis_lock_jitter);
    RUN_TEST(test_hysteresis_unlock_only_when_parked);
    RUN_TEST(test_hysteresis_warning_jitter);
    RUN_TEST(test_debounce_first_sample_immediate);
    RUN_TEST(test_debounce_noise_ignored);
    RUN_TEST(test_debounce_third_frame_accepted);
    RUN_TEST(test_debounce_counts_frames_not_steps);
    RUN_TEST(test_debounce_doors_independent);
    return TEST_SUMMARY();
}
