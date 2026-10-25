/* CAN 프레임 — 모든 모듈이 주고받는 공통 데이터 타입 (웹의 공통 DTO 같은 역할)
 *
 * 프레임 전체(SOF, CRC, ACK, EOF ...) 중 소프트웨어가 다루는 3개 필드만 담는다.
 * 나머지는 CAN 컨트롤러 하드웨어가 처리한다. (docs/notes/07-can-protocol.md 6번)
 *
 * Linux <linux/can.h>의 struct can_frame과 같은 구성이라 M5에서 변환이 쉽다.
 * 상수 이름에 CAN_FRAME_ 접두사를 붙인 이유: <linux/can.h>에 CAN_MAX_DLC 같은 이름이
 * 이미 있어서, 나중에 같이 include 하면 이름이 충돌하기 때문.
 */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAN_FRAME_MAX_DLC     8u        /* 일반 CAN의 데이터 최대 길이 (바이트) */
#define CAN_FRAME_STD_ID_MAX  0x7FFu    /* Standard ID(11비트) 최댓값 */

/* can_frame_format()에 넘길 버퍼 크기: "0x7FF [8] " + "FF " × 8 + '\0' 넉넉하게 */
#define CAN_FRAME_STR_SIZE    48u

typedef struct {
    uint32_t id;                        /* 무슨 메시지인지 (Standard: 0x000 ~ 0x7FF) */
    uint8_t  dlc;                       /* 데이터 길이 0 ~ 8 */
    uint8_t  data[CAN_FRAME_MAX_DLC];   /* 실제 값. dlc보다 뒤쪽 바이트는 의미 없음 */
} CanFrame;

/* 이 프로젝트에서 처리할 수 있는 프레임인가?
 * - f가 NULL이면 false
 * - ID가 Standard 범위(0x7FF) 밖이면 false
 * - DLC가 8보다 크면 false */
bool can_frame_is_valid(const CanFrame *f);

/* 프레임을 "0x100 [8] 10 27 00 00 00 00 00 00" 형태의 문자열로 만든다.
 * buf에 최대 size바이트까지 쓴다 (snprintf처럼 넘치면 잘림).
 * DLC가 8보다 커도 data[8] 밖은 절대 읽지 않는다. */
void can_frame_format(const CanFrame *f, char *buf, size_t size);

/* can_frame_format() 결과를 화면에 한 줄로 출력한다 */
void can_frame_print(const CanFrame *f);

#endif
