/* 2진수 출력 유틸 — 비트 연산 디버깅용 (앞으로 계속 씀) */
#ifndef BITPRINT_H
#define BITPRINT_H

#include <stdio.h>
#include <stdint.h>

/* value의 하위 nbits 비트를 2진수로 출력한다. 4비트마다 공백. 예: 1011 0100 */
static inline void print_binary(uint32_t value, int nbits) {
    for (int i = nbits - 1; i >= 0; i--) {
        putchar(((value >> i) & 1u) ? '1' : '0');
        if (i % 4 == 0 && i != 0) {
            putchar(' ');
        }
    }
}

/* "라벨 : 2진수 (0x16진수)" 한 줄 출력 */
static inline void print_bits(const char *label, uint32_t value, int nbits) {
    printf("%-22s: ", label);
    print_binary(value, nbits);
    printf("  (0x%0*X)\n", (nbits + 3) / 4, value);
}

#endif
