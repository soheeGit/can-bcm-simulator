/* 실험 5: 구조체 패딩 — 멤버 순서에 따라 크기가 달라진다 */
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>   /* offsetof */

typedef struct {
    uint8_t  a;
    uint32_t b;
    uint8_t  c;
} OrderA;

typedef struct {
    uint32_t b;
    uint8_t  a;
    uint8_t  c;
} OrderB;

/* M1-2에서 만들 CanFrame과 같은 모양 */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} CanFrameLike;

int main(void) {
    printf("[1] 멤버 크기 합 (1 + 4 + 1)  : %zu\n",
           sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint8_t));

    printf("[2] sizeof(OrderA)            : %zu\n", sizeof(OrderA));
    printf("    offsetof a / b / c        : %zu / %zu / %zu\n",
           offsetof(OrderA, a), offsetof(OrderA, b), offsetof(OrderA, c));

    printf("[3] sizeof(OrderB)            : %zu\n", sizeof(OrderB));
    printf("    offsetof b / a / c        : %zu / %zu / %zu\n",
           offsetof(OrderB, b), offsetof(OrderB, a), offsetof(OrderB, c));

    printf("[4] sizeof(CanFrameLike)      : %zu\n", sizeof(CanFrameLike));
    printf("    offsetof id / dlc / data  : %zu / %zu / %zu\n",
           offsetof(CanFrameLike, id), offsetof(CanFrameLike, dlc),
           offsetof(CanFrameLike, data));

    return 0;
}
