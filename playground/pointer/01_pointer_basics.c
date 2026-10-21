/* 실험 1: 포인터 기초 — 주소(&), 역참조(*), NULL */
#include <stdio.h>

int main(void) {
    int x = 10;
    int *p = &x;    /* p는 "x의 주소"를 담는 변수 */

    printf("[1] x의 값             : %d\n", x);
    printf("[2] x의 주소 (&x)      : %p\n", (void *)&x);
    printf("[3] p에 담긴 값 (p)    : %p\n", (void *)p);
    printf("[4] p가 가리키는 값 (*p): %d\n", *p);

    *p = 20;        /* p가 가리키는 곳에 20을 쓴다 */
    printf("[5] *p = 20 이후 x     : %d\n", x);

    printf("[6] sizeof(int)        : %zu\n", sizeof(int));
    printf("[7] sizeof(int *)      : %zu\n", sizeof(int *));
    printf("[8] sizeof(char *)     : %zu\n", sizeof(char *));

    int *q = NULL;  /* 아무것도 가리키지 않음 (Java의 null) */
    printf("[9] q == NULL ?        : %s\n", q == NULL ? "yes" : "no");

    /* 실험: 아래 주석을 풀고 make → 실행하면? */
    /* *q = 1; */

    return 0;
}
