/* clock_gettime(CLOCK_MONOTONIC)을 -std=c11에서 쓰려면 POSIX 이름을 켜야 한다 */
#define _DEFAULT_SOURCE

#include "can_dump.h"

#include <stdint.h>
#include <time.h>

#include "can_frame.h"
#include "can_socket.h"
#include "logger.h"
#include "signals.h"

/* 시작 뒤로 흐른 시간(ms). 벽시계(날짜)가 아니라 "멈추지 않고 늘기만 하는 시계"를 쓴다 */
static uint64_t monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

int can_dump_run(const char *ifname) {
    int fd = can_socket_open(ifname);
    if (fd < 0) {
        LOG_ERROR("%s 소켓을 열 수 없습니다", ifname);
        return 1;
    }
    LOG_INFO("%s에서 받는 중... (Ctrl+C로 종료)", ifname);

    const uint64_t start = monotonic_ms();
    for (;;) {
        CanFrame f;
        CanReadResult r = can_socket_read(fd, &f);
        log_set_time_ms((uint32_t)(monotonic_ms() - start));

        if (r == CAN_READ_ERROR) {
            can_socket_close(fd);
            return 1;
        }
        if (r == CAN_READ_SKIP) {
            LOG_DEBUG("무시: Extended / Remote / 에러 프레임");
            continue;
        }
        char frame_str[CAN_FRAME_STR_SIZE];
        char desc[96];
        can_frame_format(&f, frame_str, sizeof(frame_str));
        can_frame_describe(&f, desc, sizeof(desc));
        LOG_INFO("RX %-36s → %s", frame_str, desc);
    }
}
