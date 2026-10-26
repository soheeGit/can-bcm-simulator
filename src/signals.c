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
