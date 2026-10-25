/* 비트 조작 유틸 (M0-5 실험에서 만든 함수를 프로젝트 모듈로 옮김)
 *
 * C에는 namespace가 없어서, 함수 이름 앞에 모듈 이름(bit_)을 붙여 충돌을 피한다.
 * 모든 함수는 32비트 값 기준이며, 비트 번호 n은 0 ~ 31만 유효하다.
 * n >= 32 이면 시프트가 UB가 되므로(M0-6 실험 3) 아무것도 하지 않는다.
 */
#ifndef BIT_UTILS_H
#define BIT_UTILS_H

#include <stdbool.h>
#include <stdint.h>

/* n번째 비트가 1이면 true. n >= 32 이면 false */
bool bit_is_set(uint32_t x, uint32_t n);

/* n번째 비트를 1로 만든 값. n >= 32 이면 x 그대로 */
uint32_t bit_set(uint32_t x, uint32_t n);

/* n번째 비트를 0으로 만든 값. n >= 32 이면 x 그대로 */
uint32_t bit_clear(uint32_t x, uint32_t n);

/* n번째 비트를 뒤집은 값. n >= 32 이면 x 그대로 */
uint32_t bit_toggle(uint32_t x, uint32_t n);

/* 1인 비트의 개수 */
uint32_t bit_count(uint32_t x);

#endif
