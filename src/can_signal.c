#include "can_signal.h"

#include <stddef.h>

/* 하위 length비트가 1인 마스크. length = 32면 (1u << 32)가 UB라서 따로 처리 (M0-6 실험 3) */
static uint32_t low_mask(uint8_t length) {
    if (length >= 32u) {
        return 0xFFFFFFFFu;
    }
    return (1u << length) - 1u;
}

uint32_t can_get_bits(const uint8_t data[CAN_FRAME_MAX_DLC], uint8_t start_bit, uint8_t length) {
    if (data == NULL) {
        return 0u;
    }
    if (length == 0u || length > CAN_SIGNAL_MAX_LENGTH) {
        return 0u;
    }
    if ((uint32_t)start_bit + length > CAN_SIGNAL_MAX_BITS) {
        return 0u;
    }

    /* 1) 8바이트를 Little-endian으로 하나의 64비트 숫자로 합친다
     *    data[0]이 맨 아래(비트 0~7), data[7]이 맨 위(비트 56~63) */
    uint64_t word = 0u;
    for (uint32_t i = 0u; i < CAN_FRAME_MAX_DLC; i++) {
        word |= (uint64_t)data[i] << (8u * i);
    }

    /* 2) 밀어서 내리고, 마스크로 자른다 (M0-5) */
    return (uint32_t)(word >> start_bit) & low_mask(length);
}
