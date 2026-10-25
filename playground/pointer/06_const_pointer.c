/* 실험 6: const 위치에 따른 의미 차이 */
#include <stdio.h>
#include <stdint.h>

/* "이 함수는 data를 읽기만 한다"는 약속 — 앞으로 파서 함수가 전부 이 모양 */
static uint8_t first_byte(const uint8_t *data) {
    /* 실험 3: 아래 주석을 풀고 make 하면? */
    /* data[0] = 0xFF; */
    return data[0];
}

int main(void) {
    uint8_t buf1[2] = {0x11, 0x22};
    uint8_t buf2[2] = {0x33, 0x44};

    /* (A) const uint8_t *p : 가리키는 "값"을 바꿀 수 없음 */
    const uint8_t *p = buf1;
    p = buf2;                       /* 가리키는 대상 바꾸기 → OK */
    /* 실험 1: 아래 주석을 풀고 make 하면? */
    /* *p = 0x00; */

    /* (B) uint8_t *const q : 가리키는 "대상"을 바꿀 수 없음 */
    uint8_t *const q = buf1;
    *q = 0xAA;                      /* 값 바꾸기 → OK */
    /* 실험 2: 아래 주석을 풀고 make 하면? */
    /* q = buf2; */

    printf("[1] *p           : 0x%02X\n", *p);
    printf("[2] buf1[0]      : 0x%02X\n", buf1[0]);
    printf("[3] first_byte() : 0x%02X\n", first_byte(buf2));

    return 0;
}
