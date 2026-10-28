#include "log_reader.h"

#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include "log_parser.h"
#include "logger.h"

/* 줄 끝의 '\n', '\r'을 지운다 (로그에 줄을 그대로 찍기 위해) */
static void strip_newline(char *s) {
    size_t len = strlen(s);
    while (len > 0u && (s[len - 1u] == '\n' || s[len - 1u] == '\r')) {
        s[len - 1u] = '\0';
        len--;
    }
}

/* fgets가 버퍼를 꽉 채우고도 줄이 안 끝났으면, 그 줄의 나머지를 읽어서 버린다 */
static void discard_rest_of_line(FILE *in) {
    int c;
    do {
        c = fgetc(in);
    } while (c != '\n' && c != EOF);
}

/* ---------- 값이 바뀌었을 때만 INFO 로그 (매 프레임 찍으면 318줄이라 읽을 수 없다) ---------- */

static bool speed_changed(const VehicleSpeed *a, const VehicleSpeed *b) {
    return a->valid != b->valid || a->centi_kmh != b->centi_kmh;
}

static bool doors_changed(const DoorStatus *a, const DoorStatus *b) {
    return a->fl != b->fl || a->fr != b->fr || a->rl != b->rl || a->rr != b->rr || a->trunk != b->trunk;
}

static bool ign_changed(const IgnStatus *a, const IgnStatus *b) {
    return a->ign != b->ign || a->gear != b->gear;
}

/* 디스패치 결과를 사람이 읽을 수 있게 로그로 남긴다 */
static void log_dispatch(const CanFrame *f, DispatchResult r,
                         const VehicleInputs *before, const VehicleInputs *after) {
    char frame_str[CAN_FRAME_STR_SIZE];
    can_frame_format(f, frame_str, sizeof(frame_str));

    /* DEBUG: 모든 프레임 */
    LOG_DEBUG("RX %-36s → %s", frame_str, dispatch_result_name(r));

    if (r == DISPATCH_ERROR) {
        LOG_WARN("해석할 수 없는 프레임: %s", frame_str);
        return;
    }
    if (r != DISPATCH_OK) {
        return;                                     /* IGNORED는 DEBUG로만 */
    }

    /* INFO: 처음 받았거나 값이 바뀌었을 때만 */
    char buf[64];
    switch (f->id) {
        case MSG_ID_VEHICLE_SPEED:
            if (!before->speed_received || speed_changed(&before->speed, &after->speed)) {
                vehicle_speed_format(&after->speed, buf, sizeof(buf));
                LOG_INFO("차속  %s", buf);
            }
            break;
        case MSG_ID_DOOR_STATUS:
            if (!before->doors_received || doors_changed(&before->doors, &after->doors)) {
                door_status_format(&after->doors, buf, sizeof(buf));
                LOG_INFO("도어  %s", buf);
            }
            break;
        case MSG_ID_IGN_STATUS:
            if (!before->ign_received || ign_changed(&before->ign, &after->ign)) {
                ign_status_format(&after->ign, buf, sizeof(buf));
                LOG_INFO("시동  %s", buf);
            }
            break;
        default:
            break;
    }
}

int log_read_stream(FILE *in, VehicleInputs *inputs, LogReadStats *stats) {
    if (in == NULL || inputs == NULL || stats == NULL) {
        return -1;
    }
    *stats = (LogReadStats){0};

    char line[LOG_LINE_MAX];
    bool have_first_ts = false;
    uint64_t first_ts_us = 0u;

    while (fgets(line, (int)sizeof(line), in) != NULL) {
        stats->lines++;

        /* 1) 줄이 버퍼보다 긴가? fgets는 버퍼가 차면 줄 중간에서 멈춘다
         *    → 마지막 글자가 '\n'이 아니고 파일 끝도 아니면 "너무 긴 줄" */
        size_t len = strlen(line);
        if (len == sizeof(line) - 1u && line[len - 1u] != '\n' && !feof(in)) {
            discard_rest_of_line(in);
            stats->parse_errors++;
            LOG_WARN("줄 %u: ERR_TOO_LONG (%u자 초과)", stats->lines, LOG_LINE_MAX - 1u);
            continue;
        }

        /* 2) 한 줄 파싱 (M3-2) */
        CanFrame frame;
        uint64_t ts_us = 0u;
        ParseResult r = parse_candump_line(line, &frame, &ts_us);

        if (r == PARSE_SKIP) {
            stats->skipped++;
            continue;
        }
        if (r != PARSE_OK) {
            stats->parse_errors++;
            strip_newline(line);
            LOG_WARN("줄 %u: %s: %s", stats->lines, parse_result_name(r), line);
            continue;                               /* 멈추지 않고 다음 줄로 */
        }
        stats->frames++;

        /* 3) 시각: 첫 프레임을 0ms로 하는 상대 시각 (실제 candump 시각도 작은 숫자로) */
        if (!have_first_ts) {
            first_ts_us = ts_us;
            have_first_ts = true;
        }
        uint64_t rel_us = (ts_us >= first_ts_us) ? (ts_us - first_ts_us) : 0u;   /* 시간이 거꾸로 가면 0 */
        uint32_t t_ms = (uint32_t)(rel_us / 1000u);
        log_set_time_ms(t_ms);                      /* 이제부터 찍는 로그의 시각 */

        /* 4) 디스패처로 (M2-6). 바뀌었는지 비교하려고 이전 상태를 복사해 둔다 */
        VehicleInputs before = *inputs;
        DispatchResult dr = can_dispatch(&frame, inputs, t_ms, &stats->dispatch);
        log_dispatch(&frame, dr, &before, inputs);
    }

    /* fgets가 NULL을 돌려준 이유가 "파일 끝"이 아니라 "읽기 오류"인지 확인 */
    if (ferror(in)) {
        LOG_ERROR("읽기 오류: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int log_read_file(const char *path, VehicleInputs *inputs, LogReadStats *stats) {
    int result = -1;
    FILE *in = NULL;

    if (path == NULL) {
        goto cleanup;
    }
    in = fopen(path, "r");
    if (in == NULL) {
        LOG_ERROR("파일을 열 수 없습니다: %s (%s)", path, strerror(errno));
        goto cleanup;
    }

    result = log_read_stream(in, inputs, stats);

cleanup:
    /* C에는 try-finally가 없다 → 어디서 실패하든 여기 한 곳으로 모여서 정리한다 */
    if (in != NULL) {
        fclose(in);
    }
    return result;
}
