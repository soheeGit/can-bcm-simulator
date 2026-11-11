#include "frame_loop.h"

#include <stdbool.h>

#include "log_parser.h"
#include "logger.h"
#include "signals.h"

/* ---------- 값이 바뀌었을 때만 INFO 로그 (매 프레임 찍으면 318줄이라 읽을 수 없다) ---------- */

static bool speed_changed(const VehicleSpeed *a, const VehicleSpeed *b) {
    return a->valid != b->valid || a->centi_kmh != b->centi_kmh;
}

static bool doors_changed(const DoorStatus *a, const DoorStatus *b) {
    return a->fl != b->fl || a->fr != b->fr || a->rl != b->rl || a->rr != b->rr || a->trunk != b->trunk;
}

static bool ign_changed(const IgnStatus *a, const IgnStatus *b) {
    return a->ign != b->ign || a->gear != b->gear;
}

/* 디스패치 결과를 사람이 읽을 수 있게 로그로 남긴다 */
static void log_dispatch(const CanFrame *f, DispatchResult r,
                         const VehicleInputs *before, const VehicleInputs *after) {
    char frame_str[CAN_FRAME_STR_SIZE];
    can_frame_format(f, frame_str, sizeof(frame_str));

    /* DEBUG: 모든 프레임 */
    LOG_DEBUG("RX %-36s → %s", frame_str, dispatch_result_name(r));

    if (r == DISPATCH_ERROR) {
        LOG_WARN("해석할 수 없는 프레임: %s", frame_str);
        return;
    }
    if (r != DISPATCH_OK) {
        return;                                     /* IGNORED는 DEBUG로만 */
    }

    /* INFO: 처음 받았거나 값이 바뀌었을 때만 */
    char buf[64];
    switch (f->id) {
        case MSG_ID_VEHICLE_SPEED:
            if (!before->speed_received || speed_changed(&before->speed, &after->speed)) {
                vehicle_speed_format(&after->speed, buf, sizeof(buf));
                LOG_INFO("차속  %s", buf);
            }
            break;
        case MSG_ID_DOOR_STATUS:
            if (!before->doors_received || doors_changed(&before->doors, &after->doors)) {
                door_status_format(&after->doors, buf, sizeof(buf));
                LOG_INFO("도어  %s", buf);
            }
            break;
        case MSG_ID_IGN_STATUS:
            if (!before->ign_received || ign_changed(&before->ign, &after->ign)) {
                ign_status_format(&after->ign, buf, sizeof(buf));
                LOG_INFO("시동  %s", buf);
            }
            break;
        default:
            break;
    }
}

/* 끊긴 신호가 왜 못 믿게 됐는지 한 줄로 (M4-6) */
static void log_signal_lost(BcmSignal sig, const VehicleInputs *in) {
    uint32_t rx_ms = 0u;
    switch (sig) {
        case BCM_SIG_IGN:   rx_ms = in->ign_rx_ms; break;
        case BCM_SIG_SPEED:
            if (!in->speed.valid) {
                LOG_WARN("신호 이상: %s 무효값 (0xFFFF)", bcm_signal_name(sig));
                return;
            }
            rx_ms = in->speed_rx_ms;
            break;
        case BCM_SIG_DOORS: rx_ms = in->doors_rx_ms; break;
        default: break;
    }
    LOG_WARN("신호 끊김: %s (마지막 수신 %u.%03us)", bcm_signal_name(sig), rx_ms / 1000u, rx_ms % 1000u);
}

/* BCM 판단 결과를 로그로: 신호 끊김/복귀(M4-6), 상태가 바뀌었을 때, 잠금 명령을 냈을 때 */
static void log_bcm(const BcmOutputs *out, const VehicleInputs *in) {
    static const BcmSignal signals[] = { BCM_SIG_IGN, BCM_SIG_SPEED, BCM_SIG_DOORS };
    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        if ((out->newly_lost & (uint8_t)signals[i]) != 0u) {
            log_signal_lost(signals[i], in);
        }
        if ((out->recovered & (uint8_t)signals[i]) != 0u) {
            LOG_INFO("신호 복귀: %s (정상 프레임 %u개 연속)", bcm_signal_name(signals[i]), BCM_SIGNAL_RECOVERY_COUNT);
        }
    }
    if (out->state_changed) {
        LOG_INFO("STATE %s -> %s (%s: %s)", bcm_state_name(out->prev_state), bcm_state_name(out->state),
                 bcm_rule_name(out->rule), bcm_rule_reason(out->rule));
    }
    if (out->door_lock_cmd) {
        LOG_INFO("도어 잠금 명령");
    }
}

/* BCM이 보낼 0x400을 로그(DEBUG)와 파일(tx_out)로 (M4-8). 송신 프레임의 interface 이름은 vcan0으로 쓴다 */
static void emit_tx(const CanFrame *f, uint32_t t_ms, FILE *tx_out) {
    char frame_str[CAN_FRAME_STR_SIZE];
    can_frame_format(f, frame_str, sizeof(frame_str));
    LOG_DEBUG("TX %s", frame_str);
    if (tx_out != NULL) {
        char line[CANDUMP_LINE_MAX];
        if (format_candump_line(line, sizeof(line), (uint64_t)t_ms * 1000u, "vcan0", f) > 0) {
            fputs(line, tx_out);
            fflush(tx_out);                     /* 실시간 모드는 Ctrl+C로 끝나니 줄마다 내보낸다 */
        }
    }
}

int frame_loop_run(const CanSource *src, VehicleInputs *inputs, BcmContext *bcm, FILE *tx_out,
                   FrameLoopStats *stats) {
    if (src == NULL || src->read_frame == NULL || inputs == NULL || stats == NULL) {
        return -1;
    }
    *stats = (FrameLoopStats){0};

    for (;;) {
        /* 1) 소스에서 프레임 하나. 파일인지 소켓인지 여기서는 모른다 (인터페이스로만 부른다) */
        CanFrame frame = {0};
        uint32_t t_ms = 0u;
        CanSourceResult r = src->read_frame(src->self, &frame, &t_ms);   /* Java: src.readFrame(...). this(self)를 직접 넘긴다 */
        if (r == CAN_SOURCE_END) {
            return 0;
        }
        if (r != CAN_SOURCE_OK) {
            return -1;
        }
        stats->frames++;
        log_set_time_ms(t_ms);                      /* 이제부터 찍는 로그의 시각 */

        /* 2) 디스패처로 (M2-6). 바뀌었는지 비교하려고 이전 상태를 복사해 둔다 */
        VehicleInputs before = *inputs;
        DispatchResult dr = can_dispatch(&frame, inputs, t_ms, &stats->dispatch);
        log_dispatch(&frame, dr, &before, inputs);

        /* 3) BCM 판단 (M4-3). 디스패처가 inputs를 고친 "다음"에 부른다 */
        if (bcm != NULL) {
            BcmOutputs out;
            bcm_step(bcm, inputs, t_ms, &out);
            log_bcm(&out, inputs);
            if (out.state_changed) {
                if (stats->bcm_transitions < FRAME_LOOP_MAX_TRANSITIONS) {
                    BcmTransitionRecord *rec = &stats->transitions[stats->bcm_transitions];
                    rec->t_ms = t_ms;
                    rec->from = out.prev_state;
                    rec->to = out.state;
                    rec->rule = out.rule;
                }
                stats->bcm_transitions++;
            }
            if (out.door_lock_cmd) {
                stats->bcm_lock_cmds++;
            }

            /* 4) 0x400 송신 (M4-8): 100ms마다 */
            CanFrame tx;
            if (bcm_tx_poll(bcm, &out, t_ms, &tx)) {
                stats->tx_frames++;
                emit_tx(&tx, t_ms, tx_out);
            }
        }
    }
}
