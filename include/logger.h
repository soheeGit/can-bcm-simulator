/* 로거 — 레벨별 로그 출력 (Java의 SLF4J / Logback 같은 역할)
 *
 *   LOG_INFO("차속 %s", speed_str);
 *   → [   1.800s] INFO  차속 16.00 km/h
 *
 * 레벨: DEBUG < INFO < WARN < ERROR. 설정한 레벨보다 낮은 로그는 찍지 않는다.
 *   DEBUG: 모든 프레임 (자세히 보고 싶을 때)
 *   INFO : 값이 바뀔 때만 (기본)
 *   WARN : 잘못된 줄, 해석 못 한 프레임
 *   ERROR: 파일을 못 여는 등 진행할 수 없는 문제
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_OFF            /* 아무것도 안 찍음 (테스트용) */
} LogLevel;

/* 이 레벨 이상만 찍는다. 기본값: INFO */
void log_set_level(LogLevel level);
LogLevel log_get_level(void);

/* 어디에 찍을지. 기본값: stdout. NULL이면 stdout */
void log_set_output(FILE *out);

/* 로그 앞에 붙일 "시뮬레이션 시각" (ms). 로그 파일을 읽으면서 프레임마다 갱신한다 */
void log_set_time_ms(uint32_t t_ms);

/* "debug" / "info" / "warn" / "error" / "off" → LogLevel. 모르는 글자면 false */
bool log_parse_level(const char *s, LogLevel *out);

const char *log_level_name(LogLevel level);

/* 실제로 찍는 함수. 직접 부르지 말고 아래 LOG_xxx 매크로를 쓴다
 * __attribute__((format(printf, 4, 5))): printf처럼 형식 문자열과 인자 타입이 맞는지
 * 컴파일러가 검사해 준다 (예: %u에 문자열을 넘기면 경고) */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

/* 가변 인자 매크로: "..."로 받은 인자를 __VA_ARGS__로 그대로 넘긴다
 * __FILE__, __LINE__은 매크로를 쓴 자리의 파일/줄 번호 (M0-7 ASSERT_EQ와 같은 원리) */
#define LOG_DEBUG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif
