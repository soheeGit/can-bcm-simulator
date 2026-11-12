/* CAN 출력 — "프레임을 하나씩 내보내는 곳"의 공통 인터페이스 (M5-6)
 *
 * 입력에 CanSource(M5-5)가 있듯이 출력에도 같은 모양의 인터페이스를 둔다.
 * BCM이 만든 0x400을 어디로 보낼지(파일에 기록 / CAN 버스로 송신 / 테스트용 배열)는 갈아 끼운다.
 *
 *   Java: interface CanSink { boolean writeFrame(CanFrame f, int tsMs); }
 *         List<CanSink> sinks = List.of(socketSink, fileSink);   ← 여러 곳에 동시에
 */
#ifndef CAN_SINK_H
#define CAN_SINK_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"

typedef struct {
    void *self;            /* 구현 객체 (FileSink*, SocketSink* ...) */
    const char *name;      /* 로그용 이름 ("file", "socket") */
    /* 프레임 하나를 내보낸다. ts_ms는 그 프레임을 만든 시각(파일 기록용). 실패하면 false */
    bool (*write_frame)(void *self, const CanFrame *f, uint32_t ts_ms);
} CanSink;

#endif
