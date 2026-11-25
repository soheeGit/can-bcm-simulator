#include "socket_sink.h"

#include "can_socket.h"

void socket_sink_init(SocketSink *ss, int fd) {
    *ss = (SocketSink){ .fd = fd };
}

static bool socket_sink_write(void *self, const CanFrame *f, uint32_t ts_ms) {
    SocketSink *ss = self;
    (void)ts_ms;                                    /* 버스에는 "지금" 나간다. 시각은 파일 기록용 */
    if (can_socket_write(ss->fd, f) != 0) {
        ss->errors++;
        return false;
    }
    ss->sent++;
    return true;
}

CanSink socket_sink_as_can_sink(SocketSink *ss) {
    CanSink sink;
    sink.self = ss;
    sink.name = "socket";
    sink.write_frame = socket_sink_write;
    return sink;
}
