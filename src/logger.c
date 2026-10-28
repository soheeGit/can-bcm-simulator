#include "logger.h"

#include <stdarg.h>
#include <string.h>

/* 모듈 안에서만 쓰는 상태 (static = 이 파일 밖에서 안 보임, M0-7)
 * 프로그램 전체에 로거가 하나라서 전역 상태로 둔다 */
static LogLevel s_level = LOG_LEVEL_INFO;
static FILE    *s_out = NULL;            /* NULL이면 stdout */
static uint32_t s_time_ms = 0u;

void log_set_level(LogLevel level) {
    s_level = level;
}

LogLevel log_get_level(void) {
    return s_level;
}

void log_set_output(FILE *out) {
    s_out = out;
}

void log_set_time_ms(uint32_t t_ms) {
    s_time_ms = t_ms;
}

bool log_parse_level(const char *s, LogLevel *out) {
    static const struct { const char *name; LogLevel level; } table[] = {
        { "debug", LOG_LEVEL_DEBUG },
        { "info",  LOG_LEVEL_INFO  },
        { "warn",  LOG_LEVEL_WARN  },
        { "error", LOG_LEVEL_ERROR },
        { "off",   LOG_LEVEL_OFF   },
    };
    if (s == NULL || out == NULL) {
        return false;
    }
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (strcmp(s, table[i].name) == 0) {
            *out = table[i].level;
            return true;
        }
    }
    return false;
}

const char *log_level_name(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_OFF:   return "OFF";
        default:              return "?";
    }
}

/* "src/log_reader.c" → "log_reader.c" (경로 빼고 파일 이름만) */
static const char *base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level < s_level || level >= LOG_LEVEL_OFF || fmt == NULL) {
        return;                              /* 설정 레벨보다 낮으면 안 찍음 */
    }
    FILE *out = (s_out != NULL) ? s_out : stdout;

    /* [   1.800s] INFO  */
    fprintf(out, "[%4u.%03us] %-5s ", s_time_ms / 1000u, s_time_ms % 1000u, log_level_name(level));

    /* DEBUG일 때만 어느 파일 몇 번째 줄에서 찍었는지 붙인다 */
    if (level == LOG_LEVEL_DEBUG && file != NULL) {
        fprintf(out, "(%s:%d) ", base_name(file), line);
    }

    /* 가변 인자를 받아서 vfprintf로 넘긴다 (printf의 "..." 버전) */
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
}
