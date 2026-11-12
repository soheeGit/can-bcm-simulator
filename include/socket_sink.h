/* 소켓 출력 (M5-6, Linux 전용): 프레임을 CAN 버스로 송신하는 CanSink 구현
 *
 *   CanFrame ──can_frame_to_linux()──→ struct can_frame ──write()──→ vcan0
 *
 * 받는 소켓(SocketSource)의 fd를 같이 쓴다 (소켓 하나로 읽기와 쓰기를 다 한다. TCP처럼).
 * fd를 닫는 건 SocketSource가 한다.
 */
#ifndef SOCKET_SINK_H
#define SOCKET_SINK_H

#include <stdint.h>

#include "can_sink.h"

typedef struct {
    int      fd;             /* 보낼 소켓 */
    uint32_t sent;           /* 보낸 프레임 수 */
    uint32_t errors;         /* 송신 실패 수 */
} SocketSink;

void socket_sink_init(SocketSink *ss, int fd);

CanSink socket_sink_as_can_sink(SocketSink *ss);

#endif
