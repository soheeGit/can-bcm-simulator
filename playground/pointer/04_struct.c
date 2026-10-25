/* 실험 4: 구조체 — 대입, 값 전달 vs 포인터 전달, . 과 -> */
#include <stdio.h>

typedef struct {
    int x;
    int y;
} Point;

static void move_value(Point p) {
    p.x += 100;
}

static void move_pointer(Point *p) {
    p->x += 100;    /* (*p).x += 100 과 같다 */
}

int main(void) {
    Point a = {1, 2};

    /* Java라면: Point b = a; 는 같은 객체를 가리킨다. C는? */
    Point b = a;
    b.x = 99;
    printf("[1] b = a; b.x = 99 후    : a.x=%d, b.x=%d\n", a.x, b.x);

    move_value(a);
    printf("[2] move_value(a) 후      : a.x=%d\n", a.x);

    move_pointer(&a);
    printf("[3] move_pointer(&a) 후   : a.x=%d\n", a.x);

    Point *pa = &a;
    pa->y = 50;
    printf("[4] pa->y = 50 후         : a.y=%d\n", a.y);

    printf("[5] sizeof(Point)         : %zu\n", sizeof(Point));
    printf("[6] sizeof(Point *)       : %zu\n", sizeof(Point *));

    return 0;
}
