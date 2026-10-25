#include "bit_utils.h"

#define BIT_WIDTH 32u

bool bit_is_set(uint32_t x, uint32_t n) {
    if (n >= BIT_WIDTH) {
        return false;
    }
    return ((x >> n) & 1u) != 0u;
}

uint32_t bit_set(uint32_t x, uint32_t n) {
    if (n >= BIT_WIDTH) {
        return x;
    }
    return x | (1u << n);
}

uint32_t bit_clear(uint32_t x, uint32_t n) {
    if (n >= BIT_WIDTH) {
        return x;
    }
    return x & ~(1u << n);
}

uint32_t bit_toggle(uint32_t x, uint32_t n) {
    if (n >= BIT_WIDTH) {
        return x;
    }
    return x ^ (1u << n);
}

uint32_t bit_count(uint32_t x) {
    uint32_t count = 0;
    while (x != 0u) {
        count += x & 1u;
        x >>= 1;
    }
    return count;
}
