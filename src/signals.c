#include "signals.h"

#include <stdio.h>

#include "can_signal.h"

/* ---------- 0x100 VEHICLE_SPEED ---------- */

bool decode_vehicle_speed(const CanFrame *f, VehicleSpeed *out) {
    if (f == NULL || out == NULL) {
        return false;
    }
    if (!can_frame_is_valid(f)) {
        return false;
    }
    if (f->id != MSG_ID_VEHICLE_SPEED) {
        return false;                       /* 다른 메시지를 잘못 넘겼다 */
    }
    if (f->dlc < VEHICLE_SPEED_MIN_DLC) {
        return false;                       /* data[1]이 없으면 해석할 수 없다 */
    }

    uint32_t raw = can_get_bits(f->data, VEHICLE_SPEED_START_BIT, VEHICLE_SPEED_LENGTH);

    if (raw == VEHICLE_SPEED_RAW_INVALID) {
        out->valid = false;
        out->centi_kmh = 0u;
        return true;                        /* 프레임은 정상, 신호가 "모름"이라고 말한 것 */
    }

    /* factor 0.01, offset 0 → raw가 곧 0.01 km/h 단위 값 */
    out->valid = true;
    out->centi_kmh = (uint16_t)raw;
    return true;
}

void vehicle_speed_format(const VehicleSpeed *s, char *buf, size_t size) {
    if (buf == NULL || size == 0u) {
        return;
    }
    if (s == NULL || !s->valid) {
        snprintf(buf, size, "INVALID");
        return;
    }
    /* 10000 → "100" + "." + "00". float 없이 정수 나눗셈/나머지로 */
    snprintf(buf, size, "%u.%02u km/h", (unsigned)(s->centi_kmh / 100u), (unsigned)(s->centi_kmh % 100u));
}

/* ---------- 0x200 DOOR_STATUS ---------- */

/* 1비트 신호 하나를 bool로 */
static bool get_flag(const uint8_t data[CAN_FRAME_MAX_DLC], uint8_t bit) {
    return can_get_bits(data, bit, DOOR_SIGNAL_LENGTH) != 0u;
}

bool decode_door_status(const CanFrame *f, DoorStatus *out) {
    if (f == NULL || out == NULL) {
        return false;
    }
    if (!can_frame_is_valid(f)) {
        return false;
    }
    if (f->id != MSG_ID_DOOR_STATUS) {
        return false;
    }
    if (f->dlc < DOOR_STATUS_MIN_DLC) {
        return false;
    }

    out->fl    = get_flag(f->data, DOOR_FL_BIT);
    out->fr    = get_flag(f->data, DOOR_FR_BIT);
    out->rl    = get_flag(f->data, DOOR_RL_BIT);
    out->rr    = get_flag(f->data, DOOR_RR_BIT);
    out->trunk = get_flag(f->data, DOOR_TRUNK_BIT);
    return true;
}

bool door_any_open(const DoorStatus *d) {
    if (d == NULL) {
        return false;
    }
    return d->fl || d->fr || d->rl || d->rr || d->trunk;
}

void door_status_format(const DoorStatus *d, char *buf, size_t size) {
    if (buf == NULL || size == 0u) {
        return;
    }
    if (d == NULL) {
        snprintf(buf, size, "(null)");
        return;
    }
    snprintf(buf, size, "FL=%d FR=%d RL=%d RR=%d TRUNK=%d",
             d->fl, d->fr, d->rl, d->rr, d->trunk);
}
