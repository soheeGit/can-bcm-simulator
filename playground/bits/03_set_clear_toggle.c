/* 실험 3: 비트 켜기 / 끄기 / 뒤집기 — 도어 상태 바이트로 연습 */
#include <stdint.h>
#include "bitprint.h"

/* 신호 정의서 0x200 DOOR_STATUS: 비트마다 도어 하나 (1 = 열림) */
#define DOOR_FL    0u   /* 운전석 */
#define DOOR_FR    1u   /* 조수석 */
#define DOOR_RL    2u   /* 뒷좌석 왼쪽 */
#define DOOR_RR    3u   /* 뒷좌석 오른쪽 */
#define DOOR_TRUNK 4u   /* 트렁크 */

int main(void) {
    uint8_t doors = 0x00;   /* 전부 닫힘 */
    print_bits("[0] 시작 (전부 닫힘)", doors, 8);

    doors |= (uint8_t)(1u << DOOR_FL);          /* set: 운전석 열림 */
    print_bits("[1] |= 1 << FL", doors, 8);

    doors |= (uint8_t)(1u << DOOR_TRUNK);       /* set: 트렁크 열림 */
    print_bits("[2] |= 1 << TRUNK", doors, 8);

    doors &= (uint8_t)~(1u << DOOR_FL);         /* clear: 운전석 닫힘 */
    print_bits("[3] &= ~(1 << FL)", doors, 8);

    doors ^= (uint8_t)(1u << DOOR_RR);          /* toggle: 뒷좌석 오른쪽 */
    print_bits("[4] ^= 1 << RR", doors, 8);

    doors ^= (uint8_t)(1u << DOOR_RR);          /* 한 번 더 toggle */
    print_bits("[5] ^= 1 << RR (또)", doors, 8);

    printf("\n");
    printf("[6] 트렁크 열림?   : %s\n", ((doors >> DOOR_TRUNK) & 1u) ? "yes" : "no");
    printf("[7] 운전석 열림?   : %s\n", ((doors >> DOOR_FL) & 1u) ? "yes" : "no");
    printf("[8] 문이 하나라도 열림? (doors != 0) : %s\n", (doors != 0) ? "yes" : "no");

    /* 흔한 실수: clear를 할 때 ~ 를 빼먹으면? */
    uint8_t wrong = 0x1F;   /* 0001 1111: 전부 열림 */
    wrong &= (uint8_t)(1u << DOOR_FL);
    print_bits("[9] 0x1F &= (1 << FL)", wrong, 8);

    return 0;
}
