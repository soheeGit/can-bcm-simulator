/* 실험 2: 마스크 만들기와 비트 추출 — CAN 신호 파싱의 핵심 */
#include <stdint.h>
#include "bitprint.h"

int main(void) {
    /* (1) n비트짜리 마스크: (1u << n) - 1 */
    for (uint32_t n = 1; n <= 8; n++) {
        char label[32];
        snprintf(label, sizeof(label), "[1] (1u << %u) - 1", n);
        print_bits(label, (1u << n) - 1u, 8);
    }
    printf("\n");

    /* (2) n번째 비트 하나 검사: (x >> n) & 1u */
    uint8_t x = 0xB4;   /* 1011 0100 */
    print_bits("[2] x", x, 8);
    for (uint32_t n = 0; n < 8; n++) {
        printf("[2] bit %u                : %u\n", n, (x >> n) & 1u);
    }
    printf("\n");

    /* (3) 비트 2~5 (4비트) 꺼내기: 오른쪽으로 밀고 → 마스크로 자르기 */
    uint32_t shifted = (uint32_t)x >> 2;
    uint32_t mask    = (1u << 4) - 1u;
    uint32_t field   = shifted & mask;
    print_bits("[3] x >> 2", shifted, 8);
    print_bits("[3] mask (4비트)", mask, 8);
    print_bits("[3] (x >> 2) & mask", field, 8);
    printf("[3] 결과 (10진수)        : %u\n\n", field);

    /* (4) CAN 미리보기: 0x300 IGN_STATUS의 Gear 신호 (start bit 8, 길이 3) */
    uint8_t data[2] = {0x02, 0x03};   /* data[0]: IgnState=2(ON), data[1]: Gear=3(D) */
    uint32_t word = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    uint32_t ign  = (word >> 0) & ((1u << 2) - 1u);
    uint32_t gear = (word >> 8) & ((1u << 3) - 1u);
    print_bits("[4] data[1]:data[0]", word, 16);
    printf("[4] IgnState (bit 0, 2비트) : %u\n", ign);
    printf("[4] Gear     (bit 8, 3비트) : %u\n", gear);

    return 0;
}
