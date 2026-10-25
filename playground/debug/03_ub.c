/* 실험 3: 정의되지 않은 동작(UB) — 일반 빌드는 아무 말 없이 "어떤 값"을 낸다 */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

int main(void) {
    /* 컴파일러가 미리 알아채지 못하게 변수에 담아서 실행 중에 계산 */
    int one = 1;
    int shift31 = 31;
    int shift32 = 32;

    int a = one << shift31;            /* (A) signed 1을 부호 비트까지 밀기 */
    printf("[1] 1 << 31          = %d\n", a);

    int big = INT_MAX;
    int b = big + one;                 /* (B) signed 오버플로우 */
    printf("[2] INT_MAX + 1      = %d\n", b);

    uint32_t u = 1u;
    uint32_t c = u << shift32;         /* (C) 32비트 값을 32칸 밀기 (타입 폭 이상) */
    printf("[3] 1u << 32         = %u\n", c);

    printf("[끝] 프로그램이 끝까지 실행됨\n");
    return 0;
}
