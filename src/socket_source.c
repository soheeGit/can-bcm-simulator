/* clock_gettime(CLOCK_MONOTONIC)을 -std=c11에서 쓰려면 POSIX 이름을 켜야 한다 */
#define _DEFAULT_SOURCE

#include "socket_source.h"

#include <time.h>

#include "can_socket.h"
#include "logger.h"

/* 지금 시각(ms). 벽시계(날짜)가 아니라 "멈추지 않고 늘기만 하는 시계"를 쓴다 */
static uint64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int socket_source_open(SocketSource *ss, const char *ifname, const uint32_t *rx_ids, size_t rx_count) {
    *ss = (SocketSource){ .fd = -1 };
    ss->fd = can_socket_open(ifname);
    if (ss->fd < 0) {
        LOG_ERROR("%s 소켓을 열 수 없습니다", ifname != NULL ? ifname : "(null)");
        return -1;
    }
    if (rx_ids != NULL) {
        if (can_socket_set_filter(ss->fd, rx_ids, rx_count) != 0) {
            LOG_ERROR("%s 수신 필터를 걸 수 없습니다", ifname);
            socket_source_close(ss);
            return -1;
        }
        LOG_INFO("수신 필터: %zu개 ID만 받음", rx_count);
    }
    ss->start_ms = monotonic_ms();
    return 0;
}

void socket_source_close(SocketSource *ss) {
    can_socket_close(ss->fd);
    ss->fd = -1;
}

/* CanSource.read_frame의 소켓 구현 */
static CanSourceResult socket_source_read(void *self, CanFrame *out, uint32_t *ts_ms) {
    SocketSource *ss = self;
    for (;;) {                                      /* 우리 프레임이 올 때까지 */
        CanReadResult r = can_socket_read(ss->fd, out);
        *ts_ms = (uint32_t)(monotonic_ms() - ss->start_ms);
        if (r == CAN_READ_OK) {
            return CAN_SOURCE_OK;
        }
        if (r == CAN_READ_ERROR) {
            return CAN_SOURCE_ERROR;
        }
        ss->skipped++;
        log_set_time_ms(*ts_ms);
        LOG_DEBUG("무시: Extended / Remote / 에러 프레임");
    }
}

CanSource socket_source_as_can_source(SocketSource *ss) {
    CanSource src;
    src.self = ss;
    src.name = "socket";
    src.read_frame = socket_source_read;
    return src;
}
