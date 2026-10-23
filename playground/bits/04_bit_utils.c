/* 실험 4: 비트 유틸 함수 5종 + 테스트 (M0-7 테스트 프레임워크의 예고편) */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ---------- 구현 ---------- */

static bool is_bit_set(uint32_t x, uint32_t n) {
    return ((x >> n) & 1u) != 0u;
}

static uint32_t set_bit(uint32_t x, uint32_t n) {
    return x | (1u << n);
}

static uint32_t clear_bit(uint32_t x, uint32_t n) {
    return x & ~(1u << n);
}

static uint32_t toggle_bit(uint32_t x, uint32_t n) {
    return x ^ (1u << n);
}

/* 1인 비트 개수 세기: 맨 아래 비트를 보고, 오른쪽으로 한 칸씩 민다 */
static uint32_t count_bits(uint32_t x) {
    uint32_t count = 0;
    while (x != 0u) {
        count += x & 1u;
        x >>= 1;
    }
    return count;
}

/* ---------- 테스트 ---------- */

static int passed = 0;
static int failed = 0;

/* 기대값과 실제값이 다르면 몇 번째 줄에서 틀렸는지 출력 */
#define CHECK(expected, actual)                                              \
    do {                                                                     \
        unsigned long e_ = (unsigned long)(expected);                        \
        unsigned long a_ = (unsigned long)(actual);                          \
        if (e_ == a_) {                                                      \
            passed++;                                                        \
        } else {                                                             \
            failed++;                                                        \
            printf("  FAIL %s:%d  %s → 기대 0x%lX, 실제 0x%lX\n",            \
                   __FILE__, __LINE__, #actual, e_, a_);                     \
        }                                                                    \
    } while (0)

int main(void) {
    /* is_bit_set */
    CHECK(true,  is_bit_set(0xB4, 2));
    CHECK(false, is_bit_set(0xB4, 0));
    CHECK(true,  is_bit_set(0x80000000u, 31));   /* 맨 위 비트 */

    /* set_bit */
    CHECK(0x01, set_bit(0x00, 0));
    CHECK(0xB5, set_bit(0xB4, 0));
    CHECK(0xB4, set_bit(0xB4, 2));               /* 이미 1이면 그대로 */

    /* clear_bit */
    CHECK(0xB0, clear_bit(0xB4, 2));
    CHECK(0xB4, clear_bit(0xB4, 0));             /* 이미 0이면 그대로 */
    CHECK(0x7FFFFFFFu, clear_bit(0xFFFFFFFFu, 31));

    /* toggle_bit */
    CHECK(0xB5, toggle_bit(0xB4, 0));
    CHECK(0xB0, toggle_bit(0xB4, 2));
    CHECK(0xB4, toggle_bit(toggle_bit(0xB4, 5), 5));   /* 두 번 뒤집으면 원래대로 */

    /* count_bits */
    CHECK(0,  count_bits(0x00));
    CHECK(4,  count_bits(0xB4));
    CHECK(32, count_bits(0xFFFFFFFFu));

    printf("%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
