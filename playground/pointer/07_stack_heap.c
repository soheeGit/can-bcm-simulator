/* 실험 7: 스택 vs 힙 — malloc / free, 지역 변수 주소 반환의 위험 */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int x;
    int y;
} Point;

/* 힙에 만들기: 함수가 끝나도 살아 있음. 대신 직접 free 해야 함 (GC 없음!) */
static Point *make_point_heap(int x, int y) {
    Point *p = malloc(sizeof(*p));
    if (p == NULL) {
        return NULL;
    }
    p->x = x;
    p->y = y;
    return p;
}

/* 실험: 아래 함수의 주석을 풀고 make 하면?
static Point *make_point_stack(int x, int y) {
    Point p = {x, y};
    return &p;
}
*/

int main(void) {
    int local = 0;
    Point *hp = make_point_heap(3, 4);
    if (hp == NULL) {
        return 1;
    }

    printf("[1] 지역 변수 주소 (스택) : %p\n", (void *)&local);
    printf("[2] malloc 주소 (힙)      : %p\n", (void *)hp);
    printf("[3] 힙 Point              : (%d, %d)\n", hp->x, hp->y);

    free(hp);       /* 다 쓰면 반납 */
    hp = NULL;      /* 반납한 주소를 다시 쓰지 않도록 */

    return 0;
}
