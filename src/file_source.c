#include "file_source.h"

#include <errno.h>
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

void file_source_init(FileSource *fs, FILE *in) {
    *fs = (FileSource){0};
    fs->in = in;
}

/* CanSource.read_frame의 파일 구현. self는 FileSource*다 (인터페이스는 void*로만 들고 있다) */
static CanSourceResult file_source_read(void *self, CanFrame *out, uint32_t *ts_ms, int timeout_ms) {
    FileSource *fs = self;                          /* Java라면 this. C는 직접 형을 되돌린다 */
    (void)timeout_ms;                               /* 파일은 기다릴 일이 없다: 다음 줄이 바로 있다 (TIMEOUT 없음) */
    char line[FILE_SOURCE_LINE_MAX];

    /* 쓸 수 있는 프레임이 나올 때까지 줄을 읽는다 */
    while (fgets(line, (int)sizeof(line), fs->in) != NULL) {
        fs->lines++;

        /* 1) 줄이 버퍼보다 긴가? fgets는 버퍼가 차면 줄 중간에서 멈춘다
         *    → 마지막 글자가 '\n'이 아니고 파일 끝도 아니면 "너무 긴 줄" */
        size_t len = strlen(line);
        if (len == sizeof(line) - 1u && line[len - 1u] != '\n' && !feof(fs->in)) {
            discard_rest_of_line(fs->in);
            fs->parse_errors++;
            LOG_WARN("줄 %u: ERR_TOO_LONG (%u자 초과)", fs->lines, FILE_SOURCE_LINE_MAX - 1u);
            continue;
        }

        /* 2) 한 줄 파싱 (M3-2) */
        uint64_t ts_us = 0u;
        ParseResult r = parse_candump_line(line, out, &ts_us);
        if (r == PARSE_SKIP) {
            fs->skipped++;
            continue;
        }
        if (r != PARSE_OK) {
            fs->parse_errors++;
            strip_newline(line);
            LOG_WARN("줄 %u: %s: %s", fs->lines, parse_result_name(r), line);
            continue;                               /* 멈추지 않고 다음 줄로 */
        }
        fs->frames++;

        /* 3) 시각: 첫 프레임을 0ms로 하는 상대 시각 (실제 candump 시각도 작은 숫자로) */
        if (!fs->have_first_ts) {
            fs->first_ts_us = ts_us;
            fs->have_first_ts = true;
        }
        uint64_t rel_us = (ts_us >= fs->first_ts_us) ? (ts_us - fs->first_ts_us) : 0u;   /* 시간이 거꾸로 가면 0 */
        *ts_ms = (uint32_t)(rel_us / 1000u);
        return CAN_SOURCE_OK;
    }

    /* fgets가 NULL을 돌려준 이유가 "파일 끝"이 아니라 "읽기 오류"인지 확인 */
    if (ferror(fs->in)) {
        LOG_ERROR("읽기 오류: %s", strerror(errno));
        return CAN_SOURCE_ERROR;
    }
    return CAN_SOURCE_END;
}

CanSource file_source_as_can_source(FileSource *fs) {
    CanSource src;
    src.self = fs;
    src.name = "file";
    src.read_frame = file_source_read;              /* 함수 이름 = 그 함수의 주소 */
    return src;
}
