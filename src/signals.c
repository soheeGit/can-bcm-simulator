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

/* ---------- 0x300 IGN_STATUS ---------- */

/* 기어 raw 값(0~7) → Gear. 정의서에 없는 4~7은 GEAR_INVALID */
static Gear gear_from_raw(uint32_t raw) {
    switch (raw) {
        case 0u: return GEAR_P;
        case 1u: return GEAR_R;
        case 2u: return GEAR_N;
        case 3u: return GEAR_D;
        default: return GEAR_INVALID;
    }
}

bool decode_ign_status(const CanFrame *f, IgnStatus *out) {
    if (f == NULL || out == NULL) {
        return false;
    }
    if (!can_frame_is_valid(f)) {
        return false;
    }
    if (f->id != MSG_ID_IGN_STATUS) {
        return false;
    }
    if (f->dlc < IGN_STATUS_MIN_DLC) {
        return false;                       /* 기어가 data[1]에 있어서 2바이트 필요 */
    }

    /* IgnState는 2비트라 0~3이 전부 정의돼 있다 → 그대로 enum으로 */
    uint32_t ign_raw  = can_get_bits(f->data, IGN_STATE_START_BIT, IGN_STATE_LENGTH);
    uint32_t gear_raw = can_get_bits(f->data, GEAR_START_BIT, GEAR_LENGTH);

    out->ign  = (IgnState)ign_raw;
    out->gear = gear_from_raw(gear_raw);
    return true;
}

const char *ign_state_name(IgnState s) {
    switch (s) {
        case IGN_OFF:   return "OFF";
        case IGN_ACC:   return "ACC";
        case IGN_ON:    return "ON";
        case IGN_START: return "START";
        default:        return "UNKNOWN";   /* enum은 정수라 범위 밖 값이 들어올 수 있다 */
    }
}

const char *gear_name(Gear g) {
    switch (g) {
        case GEAR_P:       return "P";
        case GEAR_R:       return "R";
        case GEAR_N:       return "N";
        case GEAR_D:       return "D";
        case GEAR_INVALID: return "INVALID";
        default:           return "UNKNOWN";
    }
}

void ign_status_format(const IgnStatus *s, char *buf, size_t size) {
    if (buf == NULL || size == 0u) {
        return;
    }
    if (s == NULL) {
        snprintf(buf, size, "(null)");
        return;
    }
    snprintf(buf, size, "IGN=%s GEAR=%s", ign_state_name(s->ign), gear_name(s->gear));
}
