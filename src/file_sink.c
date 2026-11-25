#include "file_sink.h"

#include "log_parser.h"

void file_sink_init(FileSink *fs, FILE *out) {
    fs->out = out;
    fs->iface = "vcan0";
}

static bool file_sink_write(void *self, const CanFrame *f, uint32_t ts_ms) {
    FileSink *fs = self;
    char line[CANDUMP_LINE_MAX];
    if (format_candump_line(line, sizeof(line), (uint64_t)ts_ms * 1000u, fs->iface, f) <= 0) {
        return false;
    }
    if (fputs(line, fs->out) == EOF) {
        return false;
    }
    fflush(fs->out);                                /* 실시간 모드는 Ctrl+C로 끝나니 줄마다 내보낸다 */
    return true;
}

CanSink file_sink_as_can_sink(FileSink *fs) {
    CanSink sink;
    sink.self = fs;
    sink.name = "file";
    sink.write_frame = file_sink_write;
    return sink;
}
