/* 실험 4: 엔디안 — 여러 바이트짜리 숫자를 메모리에 어떤 순서로 저장할까? */
#include <stdio.h>
#include <stdint.h>

int main(void) {
    uint32_t v = 0x12345678;
    const uint8_t *p = (const uint8_t *)&v;   /* 4바이트 숫자를 1바이트씩 들여다보기 */

    printf("[1] v = 0x%08X\n", v);
    for (int i = 0; i < 4; i++) {
        printf("[2] 주소 %p (p[%d]) : 0x%02X\n", (void *)(p + i), i, p[i]);
    }

    if (p[0] == 0x78) {
        printf("[3] 이 컴퓨터는 Little-endian\n");
    } else {
        printf("[3] 이 컴퓨터는 Big-endian\n");
    }

    uint16_t s = 0x1027;
    const uint8_t *q = (const uint8_t *)&s;
    printf("[4] uint16_t 0x1027 → 메모리: 0x%02X 0x%02X\n", q[0], q[1]);

    return 0;
}
