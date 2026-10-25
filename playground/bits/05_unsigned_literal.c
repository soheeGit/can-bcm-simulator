/* 실험 5: 왜 1 이 아니라 1u 를 쓸까? */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    /* 1u: unsigned 1. 31번 비트(맨 위)까지 안전하게 밀 수 있다 */
    uint32_t good = 1u << 31;
    printf("[1] 1u << 31             : %u (0x%08X)\n", good, good);
    printf("[2] (1u << 31) > 0 ?     : %s\n", (good > 0u) ? "true" : "false");

    /* 1: signed int 1. 31번 비트는 int의 "부호 비트"라서 밀어 넣으면 UB(정의되지 않은 동작)
       아래는 컴파일러 경고를 피하려고 변수에 담아서 실행 중에 계산한다 */
    int one = 1;
    int shift = 31;
    int bad = one << shift;
    printf("[3] 1 << 31 (signed)     : %d\n", bad);
    printf("[4] (1 << 31) > 0 ?      : %s\n", (bad > 0) ? "true" : "false");

    /* M0-4 복습: uint8_t도 시프트하면 int로 승격된다 → 윗바이트가 0x80 이상이면 위험 */
    uint8_t top = 0xFF;
    uint32_t safe = (uint32_t)top << 24;
    printf("[5] (uint32_t)0xFF << 24 : 0x%08X\n", safe);

    return 0;
}
