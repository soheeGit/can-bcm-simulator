/* 실험 2: 오버플로우 — 범위를 넘으면 어떻게 될까? */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t u = 255;
    u = u + 1;
    printf("[1] uint8_t 255 + 1       : %u\n", u);

    uint8_t z = 0;
    z = z - 1;
    printf("[2] uint8_t 0 - 1         : %u\n", z);

    uint32_t a = 3, b = 5;
    printf("[3] uint32_t 3 - 5        : %u\n", a - b);

    /* CAN 차속 신호(16비트)에 범위를 넘는 값을 넣으면? */
    uint32_t speed_raw = 70000;          /* 700.00 km/h */
    uint16_t stored = (uint16_t)speed_raw;
    printf("[4] (uint16_t)70000       : %u\n", stored);

    int8_t s = 127;
    s = (int8_t)(s + 1);
    printf("[5] int8_t 127 + 1        : %d\n", s);

    /* 주의: int 같은 signed 타입이 연산 중에 넘치는 건
       "정의되지 않은 동작(UB)" — 결과를 믿으면 안 된다. M0-6에서 UBSan으로 잡아본다. */

    return 0;
}
