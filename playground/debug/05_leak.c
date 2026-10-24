/* 실험 5: 메모리 누수 (memory leak) — malloc 하고 free 를 안 함 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* CAN 프레임을 받을 때마다 버퍼를 새로 만든다고 가정 */
static uint8_t *receive_frame(void) {
    uint8_t *buf = malloc(8);
    if (buf != NULL) {
        buf[0] = 0x10;
    }
    return buf;
}

int main(void) {
    for (int i = 0; i < 100; i++) {
        uint8_t *frame = receive_frame();
        if (frame == NULL) {
            return 1;
        }
        /* 실수: 다 쓰고 free(frame); 를 안 함 */
    }
    printf("[끝] 프레임 100개 처리 완료\n");
    return 0;
}
