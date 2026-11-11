#include "can_dump.h"

#include <stdint.h>

#include "can_frame.h"
#include "can_source.h"
#include "logger.h"
#include "signals.h"
#include "socket_source.h"

int can_dump_run(const char *ifname) {
    SocketSource ss;
    if (socket_source_open(&ss, ifname) != 0) {
        return 1;
    }
    CanSource src = socket_source_as_can_source(&ss);   /* M5-5: 소켓도 CanSource로 받는다 */
    LOG_INFO("%s에서 받는 중... (Ctrl+C로 종료)", ifname);

    for (;;) {
        CanFrame f;
        uint32_t t_ms = 0u;
        if (src.read_frame(src.self, &f, &t_ms) != CAN_SOURCE_OK) {   /* 소켓은 END가 없다 → 오류 */
            socket_source_close(&ss);
            return 1;
        }
        log_set_time_ms(t_ms);
        char frame_str[CAN_FRAME_STR_SIZE];
        char desc[96];
        can_frame_format(&f, frame_str, sizeof(frame_str));
        can_frame_describe(&f, desc, sizeof(desc));
        LOG_INFO("RX %-36s → %s", frame_str, desc);
    }
}
