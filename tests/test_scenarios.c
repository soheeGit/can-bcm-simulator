/* 시나리오 통합 테스트 (M4-7)
 *
 * data/scenarios 폴더의 .log 파일을 실제 프로그램과 똑같이(파서 → 디스패처 → BCM) 끝까지 돌리고,
 * 상태 전이가 docs/fsm-design.md 9번(시나리오 대입)과 시각, 규칙까지 같은지 확인한다.
 * 단위 테스트(test_bcm, test_bcm_table)가 "부품"을 본다면, 이건 "조립된 전체"를 본다.
 *
 * make test는 저장소 맨 위 폴더에서 실행되므로 상대 경로 data/scenarios/...를 쓴다.
 */
#include <stdbool.h>
#include <stdint.h>

#include "bcm.h"
#include "log_parser.h"
#include "log_reader.h"
#include "logger.h"
#include "test_framework.h"

#define IDLE    BCM_STATE_IDLE
#define LOCKED  BCM_STATE_DRIVING_LOCKED
#define WARN    BCM_STATE_WARNING

#define MAX_EXPECTED 8u

typedef struct {
    const char *file;
    uint32_t    parse_errors;                    /* 형식이 틀린 줄 수 (04만 4) */
    uint32_t    lock_cmds;
    uint32_t    transition_count;
    BcmTransitionRecord expected[MAX_EXPECTED];  /* { 시각 ms, from, to, 규칙 } */
} ScenarioCase;

static const ScenarioCase scenarios[] = {
    { "data/scenarios/01_normal_drive.log", 0u, 1u, 2u, {
        { 1800u, IDLE,   LOCKED, BCM_RULE_T3 },
        { 9500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/02_door_open_while_driving.log", 0u, 1u, 4u, {
        { 1800u, IDLE,   LOCKED, BCM_RULE_T3 },
        { 4200u, LOCKED, WARN,   BCM_RULE_T6 },   /* 문은 4.0초에 열림, 디바운싱 3프레임 */
        { 5200u, WARN,   LOCKED, BCM_RULE_T9 },
        { 9500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/03_signal_timeout.log", 0u, 1u, 4u, {
        { 1300u, IDLE,   LOCKED, BCM_RULE_T3 },
        { 3500u, LOCKED, WARN,   BCM_RULE_T5 },   /* 2.9초 마지막 수신 + 600ms */
        { 4200u, WARN,   LOCKED, BCM_RULE_T9 },   /* 4.0초부터 정상 3프레임 */
        { 7500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/04_invalid_frames.log", 4u, 1u, 4u, {
        { 800u,  IDLE,   LOCKED, BCM_RULE_T3 },
        { 1800u, LOCKED, WARN,   BCM_RULE_T5 },   /* 차속 0xFFFF */
        { 2100u, WARN,   LOCKED, BCM_RULE_T9 },
        { 3800u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/05_speed_jitter.log", 0u, 1u, 2u, {
        { 1900u, IDLE,   LOCKED, BCM_RULE_T3 },   /* 14.90 ↔ 15.10을 30번 오가도 1번 */
        { 7500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/06_door_noise.log", 0u, 1u, 4u, {
        { 1400u, IDLE,   LOCKED, BCM_RULE_T3 },
        { 5200u, LOCKED, WARN,   BCM_RULE_T6 },   /* 3.0 ~ 4.6초 노이즈는 무시 */
        { 6200u, WARN,   LOCKED, BCM_RULE_T9 },
        { 9500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
    { "data/scenarios/07_speed_flapping.log", 0u, 1u, 4u, {
        { 1300u, IDLE,   LOCKED, BCM_RULE_T3 },
        { 3600u, LOCKED, WARN,   BCM_RULE_T5 },
        { 6200u, WARN,   LOCKED, BCM_RULE_T9 },   /* 들락날락하는 동안 경고 유지 */
        { 9500u, LOCKED, IDLE,   BCM_RULE_T7 },
    } },
};

#define SCENARIO_COUNT (sizeof(scenarios) / sizeof(scenarios[0]))

static void test_scenarios_match_design(void) {
    for (size_t i = 0; i < SCENARIO_COUNT; i++) {
        const ScenarioCase *sc = &scenarios[i];
        VehicleInputs in;
        BcmContext bcm;
        LogReadStats stats;
        vehicle_inputs_init(&in);
        bcm_init(&bcm);

        int failed_before = tf_failed;
        ASSERT_EQ(0, log_read_file(sc->file, &in, &bcm, NULL, &stats));
        ASSERT_EQ(sc->parse_errors, stats.parse_errors);
        ASSERT_EQ(sc->lock_cmds, stats.bcm_lock_cmds);
        ASSERT_EQ(sc->transition_count, stats.bcm_transitions);
        ASSERT_EQ(BCM_STATE_IDLE, bcm.state);                /* 모든 시나리오는 정차 + P로 끝난다 */

        uint32_t n = (sc->transition_count < MAX_EXPECTED) ? sc->transition_count : MAX_EXPECTED;
        for (uint32_t k = 0; k < n && k < stats.bcm_transitions; k++) {
            const BcmTransitionRecord *e = &sc->expected[k];
            const BcmTransitionRecord *a = &stats.transitions[k];
            ASSERT_EQ(e->t_ms, a->t_ms);
            ASSERT_EQ(e->from, a->from);
            ASSERT_EQ(e->to, a->to);
            ASSERT_EQ(e->rule, a->rule);
        }
        if (tf_failed != failed_before) {
            printf("         ↑ [%s]\n", sc->file);
            for (uint32_t k = 0; k < stats.bcm_transitions && k < LOG_MAX_TRANSITIONS; k++) {
                const BcmTransitionRecord *a = &stats.transitions[k];
                printf("           실제 %u.%03us %s -> %s (%s)\n", a->t_ms / 1000u, a->t_ms % 1000u,
                       bcm_state_name(a->from), bcm_state_name(a->to), bcm_rule_name(a->rule));
            }
        }
    }
}

/* ---------- 0x400 송신 round-trip (M4-8) ----------
 * 시나리오 02를 --out처럼 돌려서 0x400을 임시 파일에 쓰고 → 그 파일을 다시 파서로 읽고 → 0x400을 해석한다.
 * 쓴 쪽(BCM)이 알고 있던 값과 읽은 쪽이 해석한 값이 맞아야 한다 */
static void test_tx_round_trip(void) {
    VehicleInputs in;
    BcmContext bcm;
    LogReadStats stats;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);
    FILE *tx = tmpfile();
    ASSERT_TRUE(tx != NULL);
    if (tx == NULL) {
        return;
    }
    ASSERT_EQ(0, log_read_file("data/scenarios/02_door_open_while_driving.log", &in, &bcm, tx, &stats));
    rewind(tx);

    char line[LOG_LINE_MAX];
    uint32_t n = 0u, lock_frames = 0u, lock_at = 0u, bad = 0u;
    uint32_t first_seen[3] = {0u, 0u, 0u};      /* 상태별로 처음 보인 시각 (DRIVING_LOCKED, WARNING) */
    uint8_t prev_state = 0u;
    while (fgets(line, (int)sizeof(line), tx) != NULL) {
        CanFrame f;
        uint64_t ts = 0u;
        BcmStatusMsg m;
        if (parse_candump_line(line, &f, &ts) != PARSE_OK || !decode_bcm_status(&f, &m)) {
            bad++;                                           /* 다시 읽을 수 없는 줄 */
            continue;
        }
        uint32_t t = (uint32_t)(ts / 1000u);
        if (t != n * BCM_TX_PERIOD_MS) { bad++; }            /* 100ms마다 빠짐없이 */
        if (m.alive_counter != n % 16u) { bad++; }           /* 0, 1, ..., 15, 0, 1, ... */
        if (m.warning_buzzer != (m.state == BCM_STATE_WARNING)) { bad++; }   /* 경고음 = WARNING일 때만 */
        if (m.door_lock_cmd) { lock_frames++; lock_at = t; }
        if (m.state != prev_state && m.state < 3u && first_seen[m.state] == 0u) {
            first_seen[m.state] = t;
        }
        prev_state = m.state;
        n++;
    }
    fclose(tx);

    ASSERT_EQ(0, bad);
    ASSERT_EQ(stats.tx_frames, n);                           /* 쓴 개수 = 읽은 개수 */
    ASSERT_EQ(106, n);                                       /* 0.0 ~ 10.5초, 100ms마다 */
    ASSERT_EQ(stats.bcm_lock_cmds, lock_frames);             /* 잠금 명령이 사라지지 않고 딱 한 번 실림 */
    ASSERT_EQ(1900, lock_at);                                /* T3은 1.8초 송신 "뒤"에 일어나서 다음 송신(1.9초)에 실림 */
    ASSERT_EQ(1900, first_seen[BCM_STATE_DRIVING_LOCKED]);
    ASSERT_EQ(4300, first_seen[BCM_STATE_WARNING]);          /* T6은 4.2초 송신 뒤 → 4.3초에 보임 */
}

int main(void) {
    printf("[test_scenarios]\n");
    log_set_level(LOG_LEVEL_OFF);                            /* 실행 로그는 끄고 결과만 확인 */
    RUN_TEST(test_scenarios_match_design);
    RUN_TEST(test_tx_round_trip);
    printf("  (시나리오 %zu개)\n", SCENARIO_COUNT);
    return TEST_SUMMARY();
}
