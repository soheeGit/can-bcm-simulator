/* 실험 5: 바이트 배열 → 숫자로 조립하기 (CAN 신호 해석의 기본) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    /* 0x100 VEHICLE_SPEED 프레임의 data[0], data[1] 이라고 생각하기 */
    uint8_t data[2] = {0x10, 0x27};

    /* Little-endian(Intel): 앞 바이트가 "작은 자리" */
    uint16_t le = (uint16_t)(data[0] | (data[1] << 8));
    printf("[1] Little-endian 조립    : 0x%04X = %u\n", le, le);

    /* Big-endian(Motorola): 앞 바이트가 "큰 자리" */
    uint16_t be = (uint16_t)((data[0] << 8) | data[1]);
    printf("[2] Big-endian 조립       : 0x%04X = %u\n", be, be);

    /* 신호 정의서: VehicleSpeed = raw × 0.01 km/h (Little-endian) */
    printf("[3] 차속 (LE, ×0.01)      : %u.%02u km/h\n", le / 100, le % 100);

    /* memcpy로 그냥 붓기: 결과가 "이 컴퓨터의 엔디안"에 따라 달라진다 */
    uint16_t mc;
    memcpy(&mc, data, sizeof(mc));
    printf("[4] memcpy 결과           : 0x%04X = %u\n", mc, mc);

    /* 4바이트 Little-endian 조립: 시프트 전에 uint32_t로 바꿔야 안전하다 */
    uint8_t d4[4] = {0x78, 0x56, 0x34, 0x12};
    uint32_t v = (uint32_t)d4[0]
               | ((uint32_t)d4[1] << 8)
               | ((uint32_t)d4[2] << 16)
               | ((uint32_t)d4[3] << 24);
    printf("[5] 4바이트 LE 조립       : 0x%08X\n", v);

    return 0;
}
