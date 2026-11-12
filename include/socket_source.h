/* 소켓 소스 (M5-5, Linux 전용): SocketCAN 소켓에서 받는 프레임을 주는 CanSource 구현
 *
 *   vcan0 ──read()──→ struct can_frame ──can_frame_from_linux()──→ CanFrame + 시각(연 뒤로 흐른 ms)
 *
 * 29비트 ID / Remote / 에러 프레임은 건너뛴다 (DEBUG 로그).
 * poll()로 "프레임이 오거나 timeout_ms가 지날 때까지" 기다린다 (M5-8). 지나면 CAN_SOURCE_TIMEOUT
 * 파일과 달리 "끝"이 없다 → CAN_SOURCE_END를 돌려주지 않는다 (끝내기는 M5-9)
 */
#ifndef SOCKET_SOURCE_H
#define SOCKET_SOURCE_H

#include <stddef.h>
#include <stdint.h>

#include "can_source.h"

typedef struct {
    int      fd;             /* can_socket_open()의 결과. 닫혀 있으면 -1 */
    uint64_t start_ms;       /* 연 시각 (CLOCK_MONOTONIC). 이걸 빼서 0ms부터 시작 */
    uint32_t skipped;        /* 무시한 프레임 수 (EFF/RTR/ERR) */
} SocketSource;

/* ifname(예: "vcan0")을 연다. 0 = 성공, -1 = 실패 (ERROR 로그)
 * rx_ids: 받을 ID 목록 (M5-7 수신 필터). NULL이면 필터 없이 전부 받는다 (덤프 모드, --no-filter) */
int socket_source_open(SocketSource *ss, const char *ifname, const uint32_t *rx_ids, size_t rx_count);

/* 닫는다. 이미 닫혔으면 아무것도 안 한다 */
void socket_source_close(SocketSource *ss);

/* ss를 CanSource 인터페이스로 감싼다 */
CanSource socket_source_as_can_source(SocketSource *ss);

#endif
