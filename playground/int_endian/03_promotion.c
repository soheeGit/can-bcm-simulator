/* 실험 3: 정수 승격(integer promotion) — uint8_t끼리 계산해도 결과는 int */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint8_t a = 200, b = 100;

    printf("[1] sizeof(a)             : %zu\n", sizeof(a));
    printf("[2] sizeof(a + b)         : %zu\n", sizeof(a + b));
    printf("[3] a + b                 : %d\n", a + b);

    uint8_t sum = a + b;
    printf("[4] uint8_t sum = a + b   : %u\n", sum);

    uint8_t x = 0x80;
    printf("[5] x << 1                : %d\n", x << 1);
    uint8_t shifted = (uint8_t)(x << 1);
    printf("[6] (uint8_t)(x << 1)     : %u\n", shifted);

    /* 비트 반전 ~ 도 int로 승격된 뒤에 일어난다 */
    uint8_t m = 0x0F;
    int expected = 0xF0;
    printf("[7] ~m (int로 출력)       : %d (0x%X)\n", ~m, (unsigned)~m);
    printf("[8] ~m == 0xF0 ?          : %s\n", (~m == expected) ? "true" : "false");
    printf("[9] (uint8_t)~m == 0xF0 ? : %s\n", ((uint8_t)~m == expected) ? "true" : "false");

    return 0;
}
