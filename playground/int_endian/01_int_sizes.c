/* 실험 1: 정수 타입의 크기 — "int는 몇 바이트?"는 플랫폼마다 다르다 */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    printf("=== C 기본 타입 (크기가 플랫폼마다 다를 수 있음)\n");
    printf("[1] char      : %zu\n", sizeof(char));
    printf("[2] short     : %zu\n", sizeof(short));
    printf("[3] int       : %zu\n", sizeof(int));
    printf("[4] long      : %zu\n", sizeof(long));
    printf("[5] long long : %zu\n", sizeof(long long));
    printf("[6] void *    : %zu\n", sizeof(void *));

    printf("\n=== <stdint.h> 고정폭 타입 (이름에 크기가 박혀 있음)\n");
    printf("[7] uint8_t   : %zu\n", sizeof(uint8_t));
    printf("[8] uint16_t  : %zu\n", sizeof(uint16_t));
    printf("[9] uint32_t  : %zu\n", sizeof(uint32_t));
    printf("[10] uint64_t : %zu\n", sizeof(uint64_t));

    printf("\n=== 표현할 수 있는 범위\n");
    printf("[11] uint8_t  : 0 ~ %u\n", (unsigned)UINT8_MAX);
    printf("[12] int8_t   : %d ~ %d\n", INT8_MIN, INT8_MAX);
    printf("[13] uint16_t : 0 ~ %u\n", (unsigned)UINT16_MAX);
    printf("[14] int      : %d ~ %d\n", INT_MIN, INT_MAX);
    printf("[15] uint32_t : 0 ~ %u\n", UINT32_MAX);

    return 0;
}
