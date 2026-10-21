/* 실험 3: 배열과 포인터 — 포인터 산술, 함수에 배열 넘기기 */
#include <stdio.h>
#include <stdint.h>

/* 함수 인자에 uint8_t arr[] 라고 써도 실제로는 uint8_t *arr 와 똑같다 */
static void print_in_function(const uint8_t *arr) {
    printf("[6] 함수 안 sizeof(arr)  : %zu  (big을 넘김)\n", sizeof(arr));
}

/* 그래서 C 함수는 배열과 "길이"를 따로 받는다 */
static uint32_t sum(const uint8_t *arr, size_t len) {
    uint32_t total = 0;
    for (size_t i = 0; i < len; i++) {
        total += arr[i];
    }
    return total;
}

int main(void) {
    /* CAN 프레임의 8바이트 데이터라고 생각하기 */
    uint8_t data[8] = {0x10, 0x27, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};

    printf("[1] sizeof(data)         : %zu\n", sizeof(data));
    printf("[2] 원소 개수            : %zu\n", sizeof(data) / sizeof(data[0]));
    printf("[3] data                 : %p\n", (void *)data);
    printf("[4] &data[0]             : %p\n", (void *)&data[0]);

    for (int i = 0; i < 3; i++) {
        printf("[5] data+%d = %p | *(data+%d) = 0x%02X | data[%d] = 0x%02X\n",
               i, (void *)(data + i), i, *(data + i), i, data[i]);
    }

    uint8_t big[20] = {0};
    printf("[6] main 안 sizeof(big)  : %zu\n", sizeof(big));
    print_in_function(big);

    uint32_t values[3] = {100, 200, 300};
    for (int i = 0; i < 3; i++) {
        printf("[7] values+%d = %p\n", i, (void *)(values + i));
    }

    printf("[8] sum(data, 8)         : %u\n", sum(data, sizeof(data)));

    return 0;
}
