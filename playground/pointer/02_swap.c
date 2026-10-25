/* 실험 2: 값 전달 vs 포인터 전달 — swap 함수 */
#include <stdio.h>

static void swap_value(int a, int b) {
    int t = a;
    a = b;
    b = t;
    printf("    (swap_value 안: a=%d, b=%d)\n", a, b);
}

static void swap_pointer(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}

int main(void) {
    int x = 1, y = 2;

    swap_value(x, y);
    printf("[1] swap_value(x, y) 후    : x=%d, y=%d\n", x, y);

    swap_pointer(&x, &y);
    printf("[2] swap_pointer(&x, &y) 후: x=%d, y=%d\n", x, y);

    return 0;
}
