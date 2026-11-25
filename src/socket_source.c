/* clock_gettime(CLOCK_MONOTONIC)을 -std=c11에서 쓰려면 POSIX 이름을 켜야 한다 */
#define _DEFAULT_SOURCE

#include "socket_source.h"

#include <errno.h>
#include <poll.h>
#include <time.h>

#include "can_socket.h"
#include "logger.h"
#include "shutdown.h"

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
static CanSourceResult socket_source_read(void *self, CanFrame *out, uint32_t *ts_ms, int timeout_ms) {
    SocketSource *ss = self;
    const uint64_t deadline = monotonic_ms() + (timeout_ms > 0 ? (uint64_t)timeout_ms : 0u);
    for (;;) {                                      /* 우리 프레임이 올 때까지, 또는 시간이 다 될 때까지 */
        /* 종료 요청(Ctrl+C, M5-9)이 있으면 "끝"이라고 알린다 → 루프가 정상적으로 끝나고 main이 정리한다
         * 매 바퀴 확인: poll이 시그널에 깨어난 경우(EINTR)도, 프레임이 쉬지 않고 들어오는 경우도 여기서 잡힌다 */
        if (shutdown_requested()) {
            *ts_ms = (uint32_t)(monotonic_ms() - ss->start_ms);
            return CAN_SOURCE_END;
        }
        /* 남은 시간만큼만 기다린다 (무시할 프레임을 받고 다시 돌아와도 처음 정한 마감은 그대로) */
        int wait_ms = CAN_SOURCE_WAIT_FOREVER;
        if (timeout_ms >= 0) {
            uint64_t now = monotonic_ms();
            wait_ms = (now >= deadline) ? 0 : (int)(deadline - now);
        }
        /* fd에 읽을 게 생기거나(POLLIN) wait_ms가 지날 때까지 잠든다. CPU를 쓰지 않는다 */
        struct pollfd p = { .fd = ss->fd, .events = POLLIN };
        int n = poll(&p, 1, wait_ms);
        if (n < 0 && errno == EINTR) {              /* 시그널(Ctrl+C 등)에 깨어남 → 위로 돌아가 종료 요청 확인 */
            continue;
        }
        if (n < 0) {
            perror("poll(CAN)");
            return CAN_SOURCE_ERROR;
        }
        if (n <= 0) {
            *ts_ms = (uint32_t)(monotonic_ms() - ss->start_ms);
            return CAN_SOURCE_TIMEOUT;
        }

        CanReadResult r = can_socket_read(ss->fd, out);   /* poll이 "있다"고 했으니 기다리지 않고 바로 읽힌다 */
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
