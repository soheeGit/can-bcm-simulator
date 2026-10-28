#include "can_signal.h"

#include <stdbool.h>
#include <stddef.h>

/* 하위 length비트가 1인 마스크. length = 32면 (1u << 32)가 UB라서 따로 처리 (M0-6 실험 3) */
static uint32_t low_mask(uint8_t length) {
    if (length >= 32u) {
        return 0xFFFFFFFFu;
    }
    return (1u << length) - 1u;
}

/* get/set 공통 인자 검사 */
static bool args_valid(const uint8_t *data, uint8_t start_bit, uint8_t length) {
    if (data == NULL) {
        return false;
    }
    if (length == 0u || length > CAN_SIGNAL_MAX_LENGTH) {
        return false;
    }
    if ((uint32_t)start_bit + length > CAN_SIGNAL_MAX_BITS) {
        return false;
    }
    return true;
}

/* 8바이트 → 64비트 숫자 (Little-endian: data[0]이 맨 아래 비트 0~7) */
static uint64_t load_le64(const uint8_t data[CAN_FRAME_MAX_DLC]) {
    uint64_t word = 0u;
    for (uint32_t i = 0u; i < CAN_FRAME_MAX_DLC; i++) {
        word |= (uint64_t)data[i] << (8u * i);
    }
    return word;
}

/* 64비트 숫자 → 8바이트 (load_le64의 반대) */
static void store_le64(uint8_t data[CAN_FRAME_MAX_DLC], uint64_t word) {
    for (uint32_t i = 0u; i < CAN_FRAME_MAX_DLC; i++) {
        data[i] = (uint8_t)(word >> (8u * i));
    }
}

uint32_t can_get_bits(const uint8_t data[CAN_FRAME_MAX_DLC], uint8_t start_bit, uint8_t length) {
    if (!args_valid(data, start_bit, length)) {
        return 0u;
    }

    /* 1) 8바이트를 하나의 64비트 숫자로 합친다
     * 2) 밀어서 내리고, 마스크로 자른다 (M0-5) */
    uint64_t word = load_le64(data);
    return (uint32_t)(word >> start_bit) & low_mask(length);
}

void can_set_bits(uint8_t data[CAN_FRAME_MAX_DLC], uint8_t start_bit, uint8_t length, uint32_t value) {
    if (!args_valid(data, start_bit, length)) {
        return;
    }

    /* 그 신호 자리만 1인 마스크 (예: start 4, 길이 3 → ...0111 0000) */
    uint64_t field = (uint64_t)low_mask(length) << start_bit;

    uint64_t word = load_le64(data);
    word &= ~field;                                             /* 1) 그 자리를 0으로 지우고 (clear) */
    word |= ((uint64_t)value << start_bit) & field;             /* 2) 새 값을 그 자리에만 넣는다 (set) */
    store_le64(data, word);                                     /* 3) 다시 8바이트로 풀어 놓는다 */
}
