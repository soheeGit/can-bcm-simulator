/* CAN 신호 추출/삽입 — 신호 정의서(docs/signal-spec.md)의 "start bit + 길이"로 값을 다룬다
 *
 * 비트 번호 규칙 (정의서 1번)
 *   8바이트를 이어 붙여 0 ~ 63번. 비트 n은 data[n / 8]의 n % 8번째 비트.
 *   여러 바이트에 걸친 신호는 Intel(Little-endian): 앞 바이트(data[0])가 작은 자리.
 *
 * ADR-001(docs/notes/10-bitfield-vs-mask.md): 비트필드/memcpy 대신 이 함수로 꺼낸다.
 */
#ifndef CAN_SIGNAL_H
#define CAN_SIGNAL_H

#include <stdint.h>

#include "can_frame.h"

#define CAN_SIGNAL_MAX_BITS   (CAN_FRAME_MAX_DLC * 8u)   /* 64: 8바이트 전체 비트 수 */
#define CAN_SIGNAL_MAX_LENGTH 32u                         /* 한 신호의 최대 길이 (반환 타입 uint32_t) */

/* data[8]에서 start_bit부터 length비트를 꺼낸다 (Intel/Little-endian).
 *
 * 예: 차속(start 0, 길이 16), data = {0x10, 0x27, ...} → 0x2710 = 10000
 *
 * 잘못된 인자면 0을 반환한다:
 *   - data가 NULL
 *   - length가 0 또는 32보다 큼
 *   - start_bit + length가 64보다 큼 (8바이트 밖)
 * 인자는 정의서의 상수에서 오므로, 잘못된 인자는 "프로그래머 실수"다 → 테스트로 잡는다. */
uint32_t can_get_bits(const uint8_t data[CAN_FRAME_MAX_DLC], uint8_t start_bit, uint8_t length);

#endif
