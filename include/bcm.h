/* BCM 상태 머신 (M4)
 *
 * 규칙은 docs/fsm-design.md의 상태 전이 표(T1~T11)를 그대로 따른다.
 *
 * 입력 / 상태 / 출력을 나눈다:
 *   VehicleInputs (입력, 디스패처가 채움)  ─┐
 *   now_ms        (지금 시각)              ├─▶ bcm_step ─▶ BcmOutputs (출력, 매번 새로 채움)
 *   BcmContext    (상태, BCM의 기억)       ─┘      │
 *                    ▲                             │
 *                    └──────── 다음 상태로 고침 ────┘
 *
 * 사용법:
 *   BcmContext ctx;
 *   bcm_init(&ctx);                              // 처음 한 번
 *
 *   // 프레임을 하나 처리할 때마다 (파일 모드) 또는 주기마다 (실시간 모드, M5)
 *   can_dispatch(&frame, &inputs, now_ms, NULL);
 *   BcmOutputs out;
 *   bcm_step(&ctx, &inputs, now_ms, &out);
 *   if (out.door_lock_cmd) { ... }               // 결과 사용
 *
 * 전역 변수를 쓰지 않는다: 상태는 전부 BcmContext 안에 있고, 부르는 쪽이 들고 있는다.
 * → 테스트에서 ctx를 원하는 상태로 만들어 넣을 수 있고, BCM 두 개를 동시에 돌릴 수도 있다.
 */
#ifndef BCM_H
#define BCM_H

#include <stdbool.h>
#include <stdint.h>

#include "dispatcher.h"

/* ---------- 설정값 (fsm-design.md 3번, 7번) ---------- */

/* 히스테리시스: 들어가는 문턱과 나오는 문턱을 멀리 떨어뜨린다 (경계에서 깜빡임 방지, M4-4)
 *
 *   잠금:  IDLE → DRIVING_LOCKED   차속 ≥ BCM_LOCK_SPEED_CENTI                (T3)
 *   해제:  DRIVING_LOCKED → IDLE   차속 == BCM_STOPPED_SPEED_CENTI + P단       (T7)
 *   경고:  → WARNING               문 열림 + 차속 > BCM_MOVING_SPEED_CENTI     (T4, T6)
 *   경고 끝 (문 열린 채):  → IDLE   차속 == BCM_STOPPED_SPEED_CENTI             (T11)
 */
#define BCM_LOCK_SPEED_CENTI     1500u  /* 15.00 km/h 이상이면 자동 잠금 */
#define BCM_STOPPED_SPEED_CENTI  0u     /* 0.00 km/h = 정차 */
#define BCM_MOVING_SPEED_CENTI   300u   /* 3.00 km/h 초과면 "달리는 중" */
#define BCM_SIGNAL_TIMEOUT_MS    500u   /* 이보다 오래 못 받으면 신호 끊김 (100ms 주기 × 5) */

/* ---------- 상태 ---------- */

/* 값은 0x400 BCM_STATUS의 BcmState 신호와 같다 (signal-spec.md). 바꾸지 말 것 */
typedef enum {
    BCM_STATE_IDLE           = 0,   /* 정차/주차, 시동 꺼짐. 아무것도 안 함 */
    BCM_STATE_DRIVING_LOCKED = 1,   /* 주행 중, 문을 잠근 상태 */
    BCM_STATE_WARNING        = 2    /* 경고 중 (문 열림 주행, 신호 이상) */
} BcmState;

/* 이번 판단에서 실행된 전이 표의 줄 (로그와 테스트용). 숫자 = 표의 T 번호 */
typedef enum {
    BCM_RULE_NONE = 0,   /* 맞는 줄 없음 → 상태 유지 */
    BCM_RULE_T1,         /* 모든 상태: 시동 OFF/ACC → IDLE */
    BCM_RULE_T2,         /* IDLE: 신호 이상 → 유지 */
    BCM_RULE_T3,         /* IDLE: 시동 ON + 15 km/h 이상 + 문 닫힘 → DRIVING_LOCKED (잠금) */
    BCM_RULE_T4,         /* IDLE: 문 열림 + 3 km/h 초과 → WARNING */
    BCM_RULE_T5,         /* DRIVING_LOCKED: 신호 이상 → WARNING (Fail-safe) */
    BCM_RULE_T6,         /* DRIVING_LOCKED: 문 열림 + 3 km/h 초과 → WARNING */
    BCM_RULE_T7,         /* DRIVING_LOCKED: 0 km/h + P → IDLE */
    BCM_RULE_T8,         /* WARNING: 신호 이상 → 유지 */
    BCM_RULE_T9,         /* WARNING: 문 닫힘 + 잠갔었음 → DRIVING_LOCKED */
    BCM_RULE_T10,        /* WARNING: 문 닫힘 + 안 잠갔었음 → IDLE */
    BCM_RULE_T11,        /* WARNING: 0 km/h → IDLE */
    BCM_RULE_RESET       /* 표에 없음: ctx->state가 알 수 없는 값 → 안전 상태(IDLE)로 복귀 (default:) */
} BcmRule;

/* BCM의 기억. bcm_step이 읽고 고친다. 부르는 쪽은 직접 고치지 말 것 (테스트 제외) */
typedef struct {
    BcmState state;      /* 지금 상태 */
    bool     locked;     /* 이번 주행에서 잠금 명령을 냈나 (T3에서 켜고, IDLE로 갈 때 끔) */
    /* M4-5(디바운싱), M4-8(Alive Counter)에서 필드가 추가된다 */
} BcmContext;

/* 한 번 판단한 결과. bcm_step이 매번 처음부터 다시 채운다 (이전 값은 남지 않음) */
typedef struct {
    BcmState state;            /* 판단 후의 상태 (Moore) */
    bool     warning_buzzer;   /* state == WARNING이면 true (Moore) */
    bool     door_lock_cmd;    /* 이번 판단에서 T3이 실행됐을 때만 true (Mealy, 1회) */
    bool     state_changed;    /* 이번 판단에서 상태가 바뀌었나 (로그용) */
    BcmState prev_state;       /* 판단 전의 상태 (로그용: "IDLE -> DRIVING_LOCKED") */
    BcmRule  rule;             /* 실행된 규칙. 없으면 BCM_RULE_NONE */
} BcmOutputs;

/* ---------- 함수 ---------- */

/* 시작 상태로: IDLE, locked = false. NULL이면 아무것도 안 함 */
void bcm_init(BcmContext *ctx);

/* 판단 한 번.
 * - ctx:    BCM의 기억. 다음 상태로 고쳐진다
 * - in:     최신 입력 (읽기만 함)
 * - now_ms: 지금 시각 (VehicleInputs의 *_rx_ms와 같은 기준). 타임아웃 계산에 쓴다
 * - out:    결과를 채울 곳
 * 인자 중 하나라도 NULL이면 아무것도 안 한다 */
void bcm_step(BcmContext *ctx, const VehicleInputs *in, uint32_t now_ms, BcmOutputs *out);

/* 이름 (로그용): "IDLE", "DRIVING_LOCKED", "WARNING" / "T1" ~ "T11", "RESET", "-" */
const char *bcm_state_name(BcmState state);
const char *bcm_rule_name(BcmRule rule);

/* 규칙이 실행된 이유 (로그용): 예) T3 → "시동 ON, 15km/h 이상, 문 닫힘" */
const char *bcm_rule_reason(BcmRule rule);

#endif
