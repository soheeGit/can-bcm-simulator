/* 실험 2: 배열 범위 밖 쓰기 — 일반 빌드에서는 조용히 지나간다 */
#include <stdio.h>
#include <stdint.h>

static void print_bytes(const char *name, const uint8_t *buf, int len) {
    printf("%-7s:", name);
    for (int i = 0; i < len; i++) {
        printf(" %02X", buf[i]);
    }
    printf("\n");
}

int main(void) {
    uint8_t before[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t data[8]   = {0};                       /* CAN 데이터 8바이트 */
    uint8_t after[4]  = {0xBB, 0xBB, 0xBB, 0xBB};

    /* 실수: < 대신 <= 를 써서 data[8]까지, 총 9번 쓴다 */
    for (int i = 0; i <= 8; i++) {
        data[i] = 0xFF;
    }

    print_bytes("before", before, 4);
    print_bytes("data", data, 8);
    print_bytes("after", after, 4);
    printf("[끝] 프로그램이 끝까지 실행됨\n");

    return 0;
}
