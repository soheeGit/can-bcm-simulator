/* 실험 1: 경고 옵션 — 어떤 실수는 -Wall -Wextra 만으로는 안 잡힌다 */
#include <stdio.h>
#include <stdint.h>

static uint8_t to_byte(int value) {
    return value;                   /* (A) int → uint8_t: 값이 잘릴 수 있음 */
}

int main(void) {
    int count = 10;
    for (int i = 0; i < 3; i++) {
        int count = i * 2;          /* (B) 바깥 count와 이름이 같음 */
        printf("[1] 안쪽 count = %d\n", count);
    }
    printf("[2] 바깥 count = %d\n", count);

    printf("[3] to_byte(300) = %u\n", to_byte(300));

    int raw = 70000;
    uint16_t speed = raw;           /* (C) int → uint16_t: 값이 잘릴 수 있음 */
    printf("[4] speed = %u\n", speed);

    unsigned int len = 3;
    int idx = -1;
    if (idx < len) {                /* (D) signed와 unsigned 비교 */
        printf("[5] -1 < 3 → true\n");
    } else {
        printf("[5] -1 < 3 → false ?!\n");
    }

    return 0;
}
