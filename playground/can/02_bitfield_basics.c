/* M1-4 실험 1: 비트필드(bit-field) 기초
 *
 * 비트필드 = 구조체 멤버가 차지할 "비트 수"를 직접 정하는 문법
 *     unsigned int fl : 1;   ← fl은 1비트만 쓴다
 */
#include <stdio.h>
#include <stdint.h>

/* 도어 5개를 1비트씩 (0x200 DOOR_STATUS 모양) */
typedef struct {
    unsigned int fl    : 1;
    unsigned int fr    : 1;
    unsigned int rl    : 1;
    unsigned int rr    : 1;
    unsigned int trunk : 1;
} DoorBitsUint;

/* 같은 모양인데 바탕 타입만 uint8_t */
typedef struct {
    uint8_t fl    : 1;
    uint8_t fr    : 1;
    uint8_t rl    : 1;
    uint8_t rr    : 1;
    uint8_t trunk : 1;
} DoorBitsU8;

/* 비교용: 그냥 uint8_t 5개 */
typedef struct {
    uint8_t fl, fr, rl, rr, trunk;
} DoorBytes;

/* signed 1비트 비트필드 */
typedef struct {
    int flag : 1;
} SignedBit;

int main(void) {
    /* (1) 크기 비교 */
    printf("[1] sizeof(DoorBytes)    (uint8_t 5개)          : %zu\n", sizeof(DoorBytes));
    printf("[1] sizeof(DoorBitsUint) (unsigned int 비트필드) : %zu\n", sizeof(DoorBitsUint));
    printf("[1] sizeof(DoorBitsU8)   (uint8_t 비트필드)      : %zu\n", sizeof(DoorBitsU8));

    /* (2) 보통 변수처럼 읽고 쓰기 */
    DoorBitsU8 d = {0};
    d.fl = 1;
    d.trunk = 1;
    printf("[2] d.fl = %u, d.fr = %u, d.trunk = %u\n", d.fl, d.fr, d.trunk);

    /* (3) 1비트에 2를 넣으면? (컴파일러 경고를 피하려고 변수로 넣음) */
    int two = 2;
    int three = 3;
    d.fr = (uint8_t)two;
    d.rl = (uint8_t)three;
    printf("[3] d.fr = 2 넣은 뒤 → %u, d.rl = 3 넣은 뒤 → %u\n", d.fr, d.rl);

    /* (4) signed 1비트에 1을 넣으면? */
    SignedBit s = {0};
    int one = 1;
    s.flag = one;
    printf("[4] signed 1비트 flag = 1 넣은 뒤 → %d\n", s.flag);

    /* (5) 실험: 아래 주석을 풀고 make 하면? (비트필드의 주소를 구하기) */
    /* uint8_t *p = &d.fl; */

    return 0;
}
