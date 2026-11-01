#include "bcm.h"

#include <stddef.h>

/* ---------- 1) 입력에서 판단 조건 만들기 (fsm-design.md 3번) ---------- */

/* 전이 표에서 쓰는 조건들. 매 판단마다 VehicleInputs와 now_ms로 새로 계산한다 */
typedef struct {
    bool     ign_known;     /* 시동 신호를 믿을 수 있나 (끊기지 않음) */
    bool     ign_run;       /* 시동 ON 또는 START */
    bool     ign_stop;      /* 시동 OFF 또는 ACC */
    bool     speed_known;   /* 차속을 믿을 수 있나 (끊기지 않음 + valid) */
    bool     doors_known;   /* 도어 신호를 믿을 수 있나 */
    bool     inputs_ok;     /* 위 셋이 전부 믿을 만함 */
    bool     door_open;     /* 문이 하나라도 열림 (트렁크 포함) */
    bool     all_closed;    /* 문이 전부 닫힘 */
    uint16_t speed;         /* 차속 (centi_kmh). speed_known일 때만 의미가 있다 */
    Gear     gear;          /* 기어. ign_known일 때만 의미가 있다 */
} BcmConditions;

/* 한 번도 못 받았거나, 마지막 수신부터 TIMEOUT을 "넘겼으면" 끊김 (딱 500ms는 아직 아님) */
static bool signal_lost(bool received, uint32_t rx_ms, uint32_t now_ms) {
    if (!received) {
        return true;
    }
    uint32_t elapsed = (now_ms >= rx_ms) ? (now_ms - rx_ms) : 0u;   /* 시간이 거꾸로 가면 0으로 */
    return elapsed > BCM_SIGNAL_TIMEOUT_MS;
}

static BcmConditions evaluate(const VehicleInputs *in, uint32_t now_ms) {
    BcmConditions c;

    c.ign_known   = !signal_lost(in->ign_received, in->ign_rx_ms, now_ms);
    c.ign_run     = c.ign_known && (in->ign.ign == IGN_ON || in->ign.ign == IGN_START);
    c.ign_stop    = c.ign_known && (in->ign.ign == IGN_OFF || in->ign.ign == IGN_ACC);
    c.speed_known = !signal_lost(in->speed_received, in->speed_rx_ms, now_ms) && in->speed.valid;
    c.doors_known = !signal_lost(in->doors_received, in->doors_rx_ms, now_ms);
    c.inputs_ok   = c.ign_known && c.speed_known && c.doors_known;
    c.door_open   = c.doors_known && door_any_open(&in->doors);
    c.all_closed  = c.doors_known && !door_any_open(&in->doors);
    c.speed       = in->speed.centi_kmh;
    c.gear        = in->ign.gear;
    return c;
}

/* ---------- 2) 상태 바꾸기: 진입 동작을 한 곳에 모은다 ---------- */

/* 상태를 바꿀 때는 반드시 이 함수를 거친다.
 * "IDLE로 가면 locked를 지운다" 같은 규칙을 여기 한 곳에만 쓰면,
 * 전이 표의 줄마다 따로 쓰다가 하나 빠뜨리는 실수를 막을 수 있다 */
static void transition_to(BcmContext *ctx, BcmState next, BcmRule rule) {
    if (rule == BCM_RULE_T3) {
        ctx->locked = true;                 /* 잠금 명령을 냈다는 기억 */
    }
    if (next == BCM_STATE_IDLE) {
        ctx->locked = false;                /* 주행 끝: 잠갔다는 기억을 지운다 (T1, T7, T10, T11, RESET) */
    }
    ctx->state = next;
}

/* ---------- 3) 판단 한 번 ---------- */

void bcm_init(BcmContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->state = BCM_STATE_IDLE;
    ctx->locked = false;
}

void bcm_step(BcmContext *ctx, const VehicleInputs *in, uint32_t now_ms, BcmOutputs *out) {
    if (ctx == NULL || in == NULL || out == NULL) {
        return;
    }

    const BcmConditions c = evaluate(in, now_ms);
    const BcmState prev = ctx->state;
    BcmState next = prev;                   /* 맞는 줄이 없으면 그대로 */
    BcmRule rule = BCM_RULE_NONE;

    /* 전이 표를 위에서부터: 처음 맞는 줄 하나만 (if - else if) */
    if (c.ign_stop) {
        /* T1: 모든 상태에서 제일 먼저 */
        next = BCM_STATE_IDLE;
        rule = BCM_RULE_T1;
    } else {
        switch (prev) {
            case BCM_STATE_IDLE:
                if (!c.inputs_ok) {
                    rule = BCM_RULE_T2;                         /* 모르는 채로는 아무것도 안 함 */
                } else if (c.ign_run && c.speed >= BCM_LOCK_SPEED_CENTI && c.all_closed) {
                    next = BCM_STATE_DRIVING_LOCKED;
                    rule = BCM_RULE_T3;
                } else if (c.ign_run && c.door_open && c.speed > BCM_MOVING_SPEED_CENTI) {
                    next = BCM_STATE_WARNING;
                    rule = BCM_RULE_T4;
                }
                break;

            case BCM_STATE_DRIVING_LOCKED:
                if(!c.inputs_ok) {
                    next = BCM_STATE_WARNING;
                    rule = BCM_RULE_T5;
                } else if(c.door_open && c.speed > BCM_MOVING_SPEED_CENTI) {
                    next = BCM_STATE_WARNING;
                    rule = BCM_RULE_T6;
                } else if(c.speed == 0u && c.gear == GEAR_P) {
                    next = BCM_STATE_IDLE;
                    rule = BCM_RULE_T7;
                }
                break;

            case BCM_STATE_WARNING:
                if (!c.inputs_ok) {
                    rule = BCM_RULE_T8;                         /* 신호가 돌아올 때까지 경고 유지 */
                } else if (c.all_closed && ctx->locked) {
                    next = BCM_STATE_DRIVING_LOCKED;
                    rule = BCM_RULE_T9;
                } else if (c.all_closed) {
                    next = BCM_STATE_IDLE;
                    rule = BCM_RULE_T10;
                } else if (c.speed == 0u) {
                    next = BCM_STATE_IDLE;
                    rule = BCM_RULE_T11;
                }
                break;

            default:
                /* 메모리가 깨지는 등으로 state가 0~2가 아니면: 안전 상태로 */
                next = BCM_STATE_IDLE;
                rule = BCM_RULE_RESET;
                break;
        }
    }

    if (next != prev) {
        transition_to(ctx, next, rule);
    }

    /* 출력은 매번 처음부터 채운다 (지난번 값이 남지 않게) */
    out->prev_state     = prev;
    out->state          = ctx->state;
    out->state_changed  = (ctx->state != prev);
    out->rule           = rule;
    out->warning_buzzer = (ctx->state == BCM_STATE_WARNING);   /* Moore: 상태만 본다 */
    out->door_lock_cmd  = (rule == BCM_RULE_T3);               /* Mealy: 이번 전이만 본다 */
}

/* ---------- 이름 ---------- */

const char *bcm_state_name(BcmState state) {
    switch (state) {
        case BCM_STATE_IDLE:           return "IDLE";
        case BCM_STATE_DRIVING_LOCKED: return "DRIVING_LOCKED";
        case BCM_STATE_WARNING:        return "WARNING";
        default:                       return "UNKNOWN";
    }
}

const char *bcm_rule_name(BcmRule rule) {
    switch (rule) {
        case BCM_RULE_NONE:  return "-";
        case BCM_RULE_T1:    return "T1";
        case BCM_RULE_T2:    return "T2";
        case BCM_RULE_T3:    return "T3";
        case BCM_RULE_T4:    return "T4";
        case BCM_RULE_T5:    return "T5";
        case BCM_RULE_T6:    return "T6";
        case BCM_RULE_T7:    return "T7";
        case BCM_RULE_T8:    return "T8";
        case BCM_RULE_T9:    return "T9";
        case BCM_RULE_T10:   return "T10";
        case BCM_RULE_T11:   return "T11";
        case BCM_RULE_RESET: return "RESET";
        default:             return "?";
    }
}

const char *bcm_rule_reason(BcmRule rule) {
    switch (rule) {
        case BCM_RULE_NONE:  return "-";
        case BCM_RULE_T1:    return "시동 OFF/ACC";
        case BCM_RULE_T2:    return "신호 이상, 대기";
        case BCM_RULE_T3:    return "시동 ON, 15km/h 이상, 문 닫힘";
        case BCM_RULE_T4:    return "문 열림, 3km/h 초과";
        case BCM_RULE_T5:    return "신호 이상 (Fail-safe)";
        case BCM_RULE_T6:    return "문 열림, 3km/h 초과";
        case BCM_RULE_T7:    return "정차, P단";
        case BCM_RULE_T8:    return "신호 이상 계속";
        case BCM_RULE_T9:    return "문 닫힘, 잠금 상태로 복귀";
        case BCM_RULE_T10:   return "문 닫힘";
        case BCM_RULE_T11:   return "정차";
        case BCM_RULE_RESET: return "알 수 없는 상태, 안전 상태로";
        default:             return "?";
    }
}
