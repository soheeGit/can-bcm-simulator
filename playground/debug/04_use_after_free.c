/* 실험 4: free 한 메모리를 다시 쓰기 (use-after-free) */
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int x;
    int y;
} Point;

int main(void) {
    Point *p = malloc(sizeof(*p));
    if (p == NULL) {
        return 1;
    }
    p->x = 3;
    p->y = 4;

    free(p);                            /* 반납했는데 */

    printf("[1] free 후 p->x = %d\n", p->x);   /* 반납한 메모리를 읽음 */
    printf("[끝] 프로그램이 끝까지 실행됨\n");
    return 0;
}
