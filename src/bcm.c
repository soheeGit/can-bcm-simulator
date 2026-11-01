#include "bcm.h"

#include <stddef.h>

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
    (void)now_ms;                    /* M4-3에서 타임아웃 계산에 쓴다 (안 쓰면 -Wunused 경고) */

    BcmState prev = ctx->state;

    /* TODO(M4-3): docs/fsm-design.md 4번 전이 표(T1~T11)를 여기에 구현한다.
     * 지금은 상태를 그대로 두는 껍데기 (헤더의 사용법대로 부를 수 있는지만 확인) */
    BcmRule rule = BCM_RULE_NONE;

    /* 출력은 매번 처음부터 채운다 */
    out->prev_state     = prev;
    out->state          = ctx->state;
    out->state_changed  = (ctx->state != prev);
    out->rule           = rule;
    out->warning_buzzer = (ctx->state == BCM_STATE_WARNING);
    out->door_lock_cmd  = (rule == BCM_RULE_T3);
}

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
        case BCM_RULE_NONE: return "-";
        case BCM_RULE_T1:   return "T1";
        case BCM_RULE_T2:   return "T2";
        case BCM_RULE_T3:   return "T3";
        case BCM_RULE_T4:   return "T4";
        case BCM_RULE_T5:   return "T5";
        case BCM_RULE_T6:   return "T6";
        case BCM_RULE_T7:   return "T7";
        case BCM_RULE_T8:   return "T8";
        case BCM_RULE_T9:   return "T9";
        case BCM_RULE_T10:  return "T10";
        case BCM_RULE_T11:  return "T11";
        default:            return "?";
    }
}
