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

/* 문의 열림/닫힘은 VehicleInputs의 값(raw) 대신 디바운싱된 값으로 본다 (M4-5) */
/* 신호를 믿을 수 있나 = 한 번이라도 받았고, "못 믿음"에 걸려 있지 않음 (M4-6) */
static bool health_known(const BcmSignalHealth *h, bool received) {
    return received && !h->latched;
}

/* 문의 열림/닫힘은 VehicleInputs의 값(raw) 대신 디바운싱된 값으로 본다 (M4-5)
 * 신호를 믿을 수 있는지는 update_health()가 미리 정해 둔 ctx의 건강 상태로 본다 (M4-6) */
static BcmConditions evaluate(const BcmContext *ctx, const VehicleInputs *in, const DoorStatus *doors) {
    BcmConditions c;

    c.ign_known   = health_known(&ctx->ign_health, in->ign_received);
    c.ign_run     = c.ign_known && (in->ign.ign == IGN_ON || in->ign.ign == IGN_START);
    c.ign_stop    = c.ign_known && (in->ign.ign == IGN_OFF || in->ign.ign == IGN_ACC);
    c.speed_known = health_known(&ctx->speed_health, in->speed_received);
    c.doors_known = health_known(&ctx->doors_health, in->doors_received);
    c.inputs_ok   = c.ign_known && c.speed_known && c.doors_known;
    c.door_open   = c.doors_known && door_any_open(doors);
    c.all_closed  = c.doors_known && !door_any_open(doors);
    c.speed       = in->speed.centi_kmh;
    c.gear        = in->ign.gear;
    return c;
}

/* ---------- 2) 신호 끊김과 복귀 (M4-6) ---------- */

/* 신호 하나의 건강 상태를 갱신한다.
 * - 믿고 있던 신호가 끊기거나(timeout) 무효값이면 → 바로 "못 믿음"에 건다 (Fail-safe는 빠르게)
 * - 걸려 있는 신호는 정상 프레임이 BCM_SIGNAL_RECOVERY_COUNT개 연속으로 와야 풀린다 (복귀는 신중하게)
 * value_valid: 지금 값이 정상인가 (차속 0xFFFF면 false. 시동, 도어는 항상 true) */
static void health_update(BcmSignalHealth *h, bool received, uint32_t rx_count, uint32_t rx_ms,
                          bool value_valid, uint32_t now_ms) {
    if (!received) {
        return;                                   /* 아직 한 번도 안 옴: "모름"이지만 끊긴 건 아니다 */
    }
    const bool new_frame = (rx_count != h->seen_count);
    h->seen_count = rx_count;
    const bool timed_out = signal_lost(received, rx_ms, now_ms);

    if (!h->latched) {
        if (timed_out || !value_valid) {
            h->latched = true;                    /* 끊김 / 무효 → 바로 못 믿음 */
            h->good_count = 0u;
        }
        return;
    }

    if(timed_out) {
        h->good_count = 0u;
    } else if(new_frame) {
        if(value_valid) {
            if(h->good_count < BCM_SIGNAL_RECOVERY_COUNT) {
                h->good_count++;
            }
        } else {
            h->good_count = 0u;
        }
    }
    if(h->good_count >= BCM_SIGNAL_RECOVERY_COUNT) {
        h->latched = false;
    }
}

/* 신호 3개의 건강 상태를 갱신하고, 지금 못 믿는 신호를 비트로 돌려준다 (한 번도 못 받은 신호는 빼고) */
static uint8_t update_health(BcmContext *ctx, const VehicleInputs *in, uint32_t now_ms) {
    health_update(&ctx->ign_health, in->ign_received, in->ign_rx_count, in->ign_rx_ms, true, now_ms);
    health_update(&ctx->speed_health, in->speed_received, in->speed_rx_count, in->speed_rx_ms,
                  in->speed.valid, now_ms);
    health_update(&ctx->doors_health, in->doors_received, in->doors_rx_count, in->doors_rx_ms, true, now_ms);

    uint8_t mask = 0u;
    if (in->ign_received && ctx->ign_health.latched)     { mask |= (uint8_t)BCM_SIG_IGN; }
    if (in->speed_received && ctx->speed_health.latched) { mask |= (uint8_t)BCM_SIG_SPEED; }
    if (in->doors_received && ctx->doors_health.latched) { mask |= (uint8_t)BCM_SIG_DOORS; }
    return mask;
}

/* ---------- 3) 도어 디바운싱 (M4-5) ---------- */

/* 새 샘플(도어 프레임 하나의 값) raw가 왔을 때 한 번 부른다.
 * 같은 값이 BCM_DOOR_DEBOUNCE_COUNT번 연속으로 와야 stable을 바꾼다 */
static void debounce_update(BcmDebounced *d, bool raw) {
    if(raw == d->candidate) {
        if(d->count < BCM_DOOR_DEBOUNCE_COUNT) {
            d->count++;
        }
    } else {
        d->candidate = raw;
        d->count = 1u;
    }
    if(d->count >= BCM_DOOR_DEBOUNCE_COUNT) {
        d->stable = d->candidate;
    }
}

/* 첫 샘플: 기다리지 않고 바로 인정 (시동 직후 200ms 동안 "문 모름"이 되지 않게) */
static void debounce_seed(BcmDebounced *d, bool raw) {
    d->stable = raw;
    d->candidate = raw;
    d->count = BCM_DOOR_DEBOUNCE_COUNT;
}

/* 새 도어 프레임이 왔으면(doors_rx_count가 늘었으면) 문 5개를 각각 디바운싱한다.
 * 다른 프레임(0x100, 0x300) 때문에 불린 판단에서는 아무것도 안 한다 → "판단 횟수"가 아니라 "도어 프레임 수"를 센다 */
static void update_doors(BcmContext *ctx, const VehicleInputs *in) {
    if (!in->doors_received || in->doors_rx_count == ctx->doors_seen_count) {
        return;
    }
    ctx->doors_seen_count = in->doors_rx_count;

    const DoorStatus *raw = &in->doors;
    if (!ctx->doors_seeded) {
        debounce_seed(&ctx->door_fl, raw->fl);
        debounce_seed(&ctx->door_fr, raw->fr);
        debounce_seed(&ctx->door_rl, raw->rl);
        debounce_seed(&ctx->door_rr, raw->rr);
        debounce_seed(&ctx->door_trunk, raw->trunk);
        ctx->doors_seeded = true;
        return;
    }
    debounce_update(&ctx->door_fl, raw->fl);
    debounce_update(&ctx->door_fr, raw->fr);
    debounce_update(&ctx->door_rl, raw->rl);
    debounce_update(&ctx->door_rr, raw->rr);
    debounce_update(&ctx->door_trunk, raw->trunk);
}

/* 디바운싱된 문 상태 (판단에 쓰는 값) */
static DoorStatus debounced_doors(const BcmContext *ctx) {
    DoorStatus d;
    d.fl = ctx->door_fl.stable;
    d.fr = ctx->door_fr.stable;
    d.rl = ctx->door_rl.stable;
    d.rr = ctx->door_rr.stable;
    d.trunk = ctx->door_trunk.stable;
    return d;
}

/* ---------- 4) 상태 바꾸기: 진입 동작을 한 곳에 모은다 ---------- */

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

/* ---------- 5) 판단 한 번 ---------- */

void bcm_init(BcmContext *ctx) {
    if (ctx == NULL) {
        return;
    }
    *ctx = (BcmContext){0};                 /* 디바운싱 기억까지 전부 0 / false */
    ctx->state = BCM_STATE_IDLE;
    ctx->locked = false;
}

void bcm_step(BcmContext *ctx, const VehicleInputs *in, uint32_t now_ms, BcmOutputs *out) {
    if (ctx == NULL || in == NULL || out == NULL) {
        return;
    }

    const uint8_t lost = update_health(ctx, in, now_ms);   /* 신호 끊김 / 복귀 (M4-6) */
    update_doors(ctx, in);                  /* 새 도어 프레임이면 디바운싱 (M4-5) */
    const DoorStatus doors = debounced_doors(ctx);
    const BcmConditions c = evaluate(ctx, in, &doors);
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
                } else if(c.speed == BCM_STOPPED_SPEED_CENTI && c.gear == GEAR_P) {
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
                } else if (c.speed == BCM_STOPPED_SPEED_CENTI) {
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
    out->lost_signals   = lost;
    out->newly_lost     = (uint8_t)(lost & (uint8_t)~ctx->lost_mask);   /* 지난번엔 믿었는데 지금 못 믿음 */
    out->recovered      = (uint8_t)(ctx->lost_mask & (uint8_t)~lost);   /* 지난번엔 못 믿었는데 지금 믿음 */
    ctx->lost_mask      = lost;
}

/* ---------- 6) 0x400 송신 (M4-8) ---------- */

bool bcm_tx_poll(BcmContext *ctx, const BcmOutputs *out, uint32_t now_ms, CanFrame *frame) {
    if (ctx == NULL || out == NULL || frame == NULL) {
        return false;
    }
    if (out->door_lock_cmd) {
        ctx->lock_cmd_pending = true;          /* 송신 때가 아니어도 잊지 않게 */
    }
    if (ctx->tx_started && now_ms < ctx->next_tx_ms) {
        return false;                          /* 아직 100ms가 안 됨 */
    }

    BcmStatusMsg msg;
    msg.state          = (uint8_t)out->state;
    msg.door_lock_cmd  = ctx->lock_cmd_pending;
    msg.warning_buzzer = out->warning_buzzer;
    msg.alive_counter  = ctx->alive_counter;
    encode_bcm_status(&msg, frame);

    ctx->lock_cmd_pending = false;             /* 실어 보냈으니 끝 (1회) */
    ctx->alive_counter = (uint8_t)((ctx->alive_counter + 1u) % (BCM_STATUS_ALIVE_MAX + 1u));   /* 15 다음 0 */
    ctx->next_tx_ms = (now_ms / BCM_TX_PERIOD_MS + 1u) * BCM_TX_PERIOD_MS;                   /* 다음 100ms 칸 */
    ctx->tx_started = true;
    return true;
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
        case BCM_RULE_T9:    return "경고 원인 해소 (신호 정상, 문 닫힘), 잠금 상태로";
        case BCM_RULE_T10:   return "경고 원인 해소 (신호 정상, 문 닫힘)";
        case BCM_RULE_T11:   return "정차";
        case BCM_RULE_RESET: return "알 수 없는 상태, 안전 상태로";
        default:             return "?";
    }
}

const char *bcm_signal_name(BcmSignal sig) {
    switch (sig) {
        case BCM_SIG_IGN:   return "시동";
        case BCM_SIG_SPEED: return "차속";
        case BCM_SIG_DOORS: return "도어";
        default:            return "?";
    }
}
