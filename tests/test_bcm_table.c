/* BCM 전이 표 테스트 (M4-7, table-driven)
 *
 * docs/fsm-design.md 4번 전이 표(T1~T11)를 그대로 배열로 옮겼다.
 * 한 줄 = { 시작 상태, 입력, 기대 결과 } = 테스트 하나. JUnit의 @ParameterizedTest와 같다.
 * 규칙을 바꾸면: fsm-design.md → 이 표 → bcm.c 순서로 고친다.
 */
#include <stdbool.h>
#include <stdint.h>

#include "bcm.h"
#include "test_framework.h"

#define NOW 10000u

/* ---------- 입력을 짧게 쓰기 위한 표기 ---------- */

/* 문 (DoorStatus의 비트 순서와 같다: signal-spec 0x200) */
#define CLOSED 0x00u
#define FL     0x01u
#define FR     0x02u
#define RL     0x04u
#define RR     0x08u
#define TRUNK  0x10u

/* 신호 이상 */
#define OK            0x00u
#define SPEED_LOST    0x01u   /* 차속 600ms째 안 옴 */
#define SPEED_INVALID 0x02u   /* 차속 0xFFFF */
#define DOORS_LOST    0x04u   /* 도어 600ms째 안 옴 */
#define IGN_LOST      0x08u   /* 시동 600ms째 안 옴 */
#define NOTHING_YET   0x10u   /* 시동 직후: 아직 아무 신호도 안 옴 */

/* 차속 (km/h → centi) */
#define KMH(x) ((uint16_t)((x) * 100u))

typedef struct {
    const char *name;        /* 실패했을 때 찍을 설명 */
    /* 시작 (given) */
    BcmState    from;
    bool        locked;
    /* 입력 (when) */
    IgnState    ign;
    Gear        gear;
    uint16_t    speed;       /* centi_kmh */
    uint8_t     doors;       /* FL | RR ... */
    uint8_t     faults;      /* SPEED_LOST | ... */
    /* 기대 (then) */
    BcmState    to;
    BcmRule     rule;
    bool        lock_cmd;
} BcmCase;

/* ---------- 전이 표 ---------- */

#define IDLE    BCM_STATE_IDLE
#define LOCKED  BCM_STATE_DRIVING_LOCKED
#define WARN    BCM_STATE_WARNING

static const BcmCase cases[] = {
    /* name                                        from    lock   ign        gear          speed      doors   faults         to      rule              lock_cmd */

    /* T1: 모든 상태 + 시동 OFF/ACC → IDLE */
    { "T1 잠금 주행 중 시동 OFF",                  LOCKED, true,  IGN_OFF,   GEAR_D,       KMH(50u),  CLOSED, OK,            IDLE,   BCM_RULE_T1,      false },
    { "T1 경고 중 ACC",                            WARN,   true,  IGN_ACC,   GEAR_D,       KMH(20u),  FL,     OK,            IDLE,   BCM_RULE_T1,      false },
    { "T1 IDLE에서 시동 OFF (상태는 그대로)",       IDLE,   false, IGN_OFF,   GEAR_P,       0u,        CLOSED, OK,            IDLE,   BCM_RULE_T1,      false },
    { "T1 시동 OFF + 차속 끊김 (T1이 T8보다 위)",   WARN,   true,  IGN_OFF,   GEAR_D,       KMH(50u),  CLOSED, SPEED_LOST,    IDLE,   BCM_RULE_T1,      false },

    /* IDLE: T2 ~ T4 */
    { "T2 시동 직후 아무 신호 없음",               IDLE,   false, IGN_OFF,   GEAR_P,       0u,        CLOSED, NOTHING_YET,   IDLE,   BCM_RULE_T2,      false },
    { "T2 차속 0xFFFF면 잠그지 않음",               IDLE,   false, IGN_ON,    GEAR_D,       KMH(50u),  CLOSED, SPEED_INVALID, IDLE,   BCM_RULE_T2,      false },
    { "T3 16 km/h 문 닫힘 → 잠금",                 IDLE,   false, IGN_ON,    GEAR_D,       KMH(16u),  CLOSED, OK,            LOCKED, BCM_RULE_T3,      true  },
    { "T3 경계 15.00 km/h (이상)",                 IDLE,   false, IGN_ON,    GEAR_D,       1500u,     CLOSED, OK,            LOCKED, BCM_RULE_T3,      true  },
    { "-  14.99 km/h는 아직",                      IDLE,   false, IGN_ON,    GEAR_D,       1499u,     CLOSED, OK,            IDLE,   BCM_RULE_NONE,    false },
    { "T3 START도 시동 ON으로",                    IDLE,   false, IGN_START, GEAR_D,       KMH(16u),  CLOSED, OK,            LOCKED, BCM_RULE_T3,      true  },
    { "T4 문 열고 출발",                           IDLE,   false, IGN_ON,    GEAR_D,       KMH(16u),  FL,     OK,            WARN,   BCM_RULE_T4,      false },
    { "-  경계 3.00 km/h (초과 아님)",              IDLE,   false, IGN_ON,    GEAR_D,       300u,      FL,     OK,            IDLE,   BCM_RULE_NONE,    false },

    /* DRIVING_LOCKED: T5 ~ T7 */
    { "T5 차속 끊김 (Fail-safe)",                  LOCKED, true,  IGN_ON,    GEAR_D,       KMH(50u),  CLOSED, SPEED_LOST,    WARN,   BCM_RULE_T5,      false },
    { "T5 차속 0xFFFF",                            LOCKED, true,  IGN_ON,    GEAR_D,       KMH(50u),  CLOSED, SPEED_INVALID, WARN,   BCM_RULE_T5,      false },
    { "T5 도어 끊김",                              LOCKED, true,  IGN_ON,    GEAR_D,       KMH(50u),  CLOSED, DOORS_LOST,    WARN,   BCM_RULE_T5,      false },
    { "T5 시동 끊김",                              LOCKED, true,  IGN_ON,    GEAR_D,       KMH(50u),  CLOSED, IGN_LOST,      WARN,   BCM_RULE_T5,      false },
    { "T5 문 열림 + 차속 끊김 (T5가 T6보다 위)",    LOCKED, true,  IGN_ON,    GEAR_D,       KMH(50u),  FL,     SPEED_LOST,    WARN,   BCM_RULE_T5,      false },
    { "T6 트렁크 열림 40 km/h",                    LOCKED, true,  IGN_ON,    GEAR_D,       KMH(40u),  TRUNK,  OK,            WARN,   BCM_RULE_T6,      false },
    { "-  문 열림 2 km/h는 유지",                  LOCKED, true,  IGN_ON,    GEAR_D,       KMH(2u),   FL,     OK,            LOCKED, BCM_RULE_NONE,    false },
    { "T7 정차 + P → IDLE",                       LOCKED, true,  IGN_ON,    GEAR_P,       0u,        CLOSED, OK,            IDLE,   BCM_RULE_T7,      false },
    { "-  정차 + D는 신호 대기",                    LOCKED, true,  IGN_ON,    GEAR_D,       0u,        CLOSED, OK,            LOCKED, BCM_RULE_NONE,    false },
    { "-  정차 + 기어 무효는 유지",                 LOCKED, true,  IGN_ON,    GEAR_INVALID, 0u,        CLOSED, OK,            LOCKED, BCM_RULE_NONE,    false },
    { "-  잠금 주행 중 다시 16 km/h (재잠금 없음)", LOCKED, true,  IGN_ON,    GEAR_D,       KMH(16u),  CLOSED, OK,            LOCKED, BCM_RULE_NONE,    false },

    /* WARNING: T8 ~ T11 */
    { "T8 신호 이상이면 경고 유지", WARN, true, IGN_ON, GEAR_D, KMH(50u), CLOSED, SPEED_LOST, WARN, BCM_RULE_T8, false },
    { "T9 40km/h, 문 전부 닫힘",  WARN, true, IGN_ON, GEAR_D, KMH(40u), CLOSED, OK,  BCM_STATE_DRIVING_LOCKED, BCM_RULE_T9, false },
    { "T10 20km/h, 문 전부 닫힘",  WARN, false, IGN_ON, GEAR_D, KMH(20u), CLOSED, OK,  BCM_STATE_IDLE, BCM_RULE_T10, false },
    { "T11 0km/h, FL 열림",  WARN, false, IGN_ON, GEAR_P, KMH(0u), FL, OK,  BCM_STATE_IDLE, BCM_RULE_T11, false },

    { "-  문 열린 채 2 km/h는 경고 유지",           WARN,   true,  IGN_ON,    GEAR_D,       KMH(2u),   FL,     OK,            WARN,   BCM_RULE_NONE,    false },

    /* 표에 없는 경우: 알 수 없는 상태 → IDLE */
    { "RESET 상태 값이 깨짐 (7)",                  (BcmState)7, true, IGN_ON, GEAR_D,       KMH(50u),  CLOSED, OK,            IDLE,   BCM_RULE_RESET,   false },
};

#define CASE_COUNT (sizeof(cases) / sizeof(cases[0]))

/* ---------- 표 한 줄 → 입력 만들기 ---------- */

static VehicleInputs make_inputs(const BcmCase *c) {
    VehicleInputs in;
    vehicle_inputs_init(&in);
    if ((c->faults & NOTHING_YET) != 0u) {
        return in;                                   /* 아무것도 안 받음 */
    }

    in.ign.ign = c->ign;
    in.ign.gear = c->gear;
    in.speed.valid = (c->faults & SPEED_INVALID) == 0u;
    in.speed.centi_kmh = c->speed;
    in.doors.fl    = (c->doors & FL) != 0u;
    in.doors.fr    = (c->doors & FR) != 0u;
    in.doors.rl    = (c->doors & RL) != 0u;
    in.doors.rr    = (c->doors & RR) != 0u;
    in.doors.trunk = (c->doors & TRUNK) != 0u;

    /* 세 신호 모두 받은 적 있음. 끊긴 신호는 600ms 전에 받은 것으로 */
    in.ign_received = true;
    in.speed_received = true;
    in.doors_received = true;
    in.ign_rx_ms   = ((c->faults & IGN_LOST) != 0u)   ? NOW - 600u : NOW;
    in.speed_rx_ms = ((c->faults & SPEED_LOST) != 0u) ? NOW - 600u : NOW;
    in.doors_rx_ms = ((c->faults & DOORS_LOST) != 0u) ? NOW - 600u : NOW;
    in.ign_rx_count = 1u;
    in.speed_rx_count = 1u;
    in.doors_rx_count = 1u;
    return in;
}

/* ---------- 테스트 ---------- */

static void test_transition_table(void) {
    for (size_t i = 0; i < CASE_COUNT; i++) {
        const BcmCase *c = &cases[i];
        BcmContext ctx;
        bcm_init(&ctx);
        ctx.state = c->from;                         /* given */
        ctx.locked = c->locked;
        VehicleInputs in = make_inputs(c);
        BcmOutputs out;

        bcm_step(&ctx, &in, NOW, &out);              /* when */

        int failed_before = tf_failed;               /* then */
        ASSERT_EQ(c->to, out.state);
        ASSERT_EQ(c->rule, out.rule);
        ASSERT_EQ(c->lock_cmd, out.door_lock_cmd);
        ASSERT_EQ(c->to == BCM_STATE_WARNING, out.warning_buzzer);   /* Moore: WARNING이면 경고음 */
        ASSERT_EQ(c->to != c->from, out.state_changed);
        if (tf_failed != failed_before) {
            printf("         ↑ 표 %zu번째 줄 [%s]\n", i + 1u, c->name);
        }
    }
}

/* 표에 T1 ~ T11과 RESET이 전부 한 번 이상 있는지 (fsm-design.md의 모든 줄이 테스트되는지) */
static void test_table_covers_every_rule(void) {
    bool seen[BCM_RULE_RESET + 1] = {false};
    for (size_t i = 0; i < CASE_COUNT; i++) {
        seen[cases[i].rule] = true;
    }
    for (int r = BCM_RULE_T1; r <= BCM_RULE_RESET; r++) {
        if (!seen[r]) {
            printf("    표에 %s가 없음\n", bcm_rule_name((BcmRule)r));
        }
        ASSERT_TRUE(seen[r]);
    }
}

int main(void) {
    printf("[test_bcm_table]\n");
    RUN_TEST(test_transition_table);
    RUN_TEST(test_table_covers_every_rule);
    printf("  (전이 표 %zu줄)\n", CASE_COUNT);
    return TEST_SUMMARY();
}
