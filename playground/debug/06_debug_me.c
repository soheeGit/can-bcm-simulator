/* 실험 6: 디버거 연습 — 차속이 100.00 km/h가 나와야 하는데 이상한 값이 나온다.
   신호 정의서: 0x100 VehicleSpeed, start bit 0, 16비트, Little-endian, ×0.01 km/h
   디버거로 한 줄씩 따라가며 어디서 틀렸는지 찾아보기 */
#include <stdio.h>
#include <stdint.h>

static uint16_t decode_speed_raw(const uint8_t *data) {
    uint16_t lo = data[0];
    uint16_t hi = data[1];
    uint16_t raw = (uint16_t)((lo << 8) | hi);
    return raw;
}

int main(void) {
    uint8_t frame[8] = {0x10, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint16_t raw = decode_speed_raw(frame);
    unsigned int kmh_int = raw / 100u;
    unsigned int kmh_frac = raw % 100u;

    printf("차속: %u.%02u km/h (기대값: 100.00 km/h)\n", kmh_int, kmh_frac);
    return 0;
}
