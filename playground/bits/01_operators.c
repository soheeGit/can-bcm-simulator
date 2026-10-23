/* 실험 1: 비트 연산자 6종 — & | ^ ~ << >> */
#include <stdint.h>
#include "bitprint.h"

int main(void) {
    uint8_t a = 0xB4;   /* 1011 0100 */
    uint8_t b = 0x0F;   /* 0000 1111 */

    print_bits("[0] a", a, 8);
    print_bits("[0] b", b, 8);
    printf("\n");

    print_bits("[1] a & b  (AND)", (uint8_t)(a & b), 8);
    print_bits("[2] a | b  (OR)", (uint8_t)(a | b), 8);
    print_bits("[3] a ^ b  (XOR)", (uint8_t)(a ^ b), 8);
    print_bits("[4] ~a     (NOT)", (uint8_t)~a, 8);
    print_bits("[5] a << 1", (uint8_t)(a << 1), 8);
    print_bits("[6] a >> 2", (uint8_t)(a >> 2), 8);

    printf("\n");
    printf("[7] a << 1 (uint8_t로 자르기 전) : %d\n", a << 1);
    printf("[8] a >> 2 를 10진수로          : %d  (180 / 4 = ?)\n", a >> 2);

    return 0;
}
