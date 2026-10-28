#include "dispatcher.h"

#include <stddef.h>

void vehicle_inputs_init(VehicleInputs *in) {
    if (in == NULL) {
        return;
    }
    *in = (VehicleInputs){0};           /* 전부 0 / false */
    in->speed.valid = false;
    in->ign.ign = IGN_OFF;
    in->ign.gear = GEAR_INVALID;        /* 아직 기어를 모른다 */
}

static void count(DispatchStats *stats, DispatchResult r) {
    if (stats == NULL) {
        return;
    }
    switch (r) {
        case DISPATCH_OK:      stats->ok++;      break;
        case DISPATCH_IGNORED: stats->ignored++; break;
        case DISPATCH_ERROR:   stats->error++;   break;
        default:                                 break;
    }
}

/* ID에 맞는 디코더를 부른다. 디코딩은 임시 변수에 먼저 하고, 성공했을 때만 in에 반영한다 */
static DispatchResult route(const CanFrame *f, VehicleInputs *in, uint32_t now_ms) {
    switch (f->id) {
        case MSG_ID_VEHICLE_SPEED: {
            VehicleSpeed s;
            if (!decode_vehicle_speed(f, &s)) {
                return DISPATCH_ERROR;
            }
            in->speed = s;
            in->speed_received = true;
            in->speed_rx_ms = now_ms;
            return DISPATCH_OK;
        }
        case MSG_ID_DOOR_STATUS: {
            DoorStatus d;
            if (!decode_door_status(f, &d)) {
                return DISPATCH_ERROR;
            }
            in->doors = d;
            in->doors_received = true;
            in->doors_rx_ms = now_ms;
            return DISPATCH_OK;
        }
        case MSG_ID_IGN_STATUS: {
            IgnStatus g;
            if (!decode_ign_status(f, &g)) {
                return DISPATCH_ERROR;
            }
            in->ign = g;
            in->ign_received = true;
            in->ign_rx_ms = now_ms;
            return DISPATCH_OK;
        }
        default:
            /* 0x400(우리가 보낸 BCM_STATUS)를 포함해, 관심 없는 메시지 */
            return DISPATCH_IGNORED;
    }
}

DispatchResult can_dispatch(const CanFrame *f, VehicleInputs *in, uint32_t now_ms,
                            DispatchStats *stats) {
    DispatchResult r;
    if (f == NULL || in == NULL || !can_frame_is_valid(f)) {
        r = DISPATCH_ERROR;
    } else {
        r = route(f, in, now_ms);
    }
    count(stats, r);
    return r;
}

const char *dispatch_result_name(DispatchResult r) {
    switch (r) {
        case DISPATCH_OK:      return "OK";
        case DISPATCH_IGNORED: return "IGNORED";
        case DISPATCH_ERROR:   return "ERROR";
        default:               return "UNKNOWN";
    }
}
