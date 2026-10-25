/* 실험 6: printf 포맷 — 타입에 맞는 포맷을 써야 한다 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>   /* PRIu16, PRIx32 등 */

int main(void) {
    uint8_t  b = 7;
    uint16_t speed = 10000;
    uint32_t id = 0x100;
    int      neg = -1;

    printf("[1] %%u   : %u\n", b);
    printf("[2] %%02X : %02X\n", b);          /* 2자리, 빈자리는 0 */
    printf("[3] %%X   : %X\n", b);
    printf("[4] %%03X : %03X\n", id);         /* CAN ID 표기에 자주 씀 */

    printf("[5] PRIu16: %" PRIu16 "\n", speed);
    printf("[6] PRIx32: 0x%" PRIx32 "\n", id);

    printf("[7] -1을 %%d로 : %d\n", neg);
    printf("[8] -1을 %%u로 : %u\n", (unsigned)neg);
    printf("[9] -1을 %%X로 : %X\n", (unsigned)neg);

    return 0;
}
