/* candump 로그 한 줄 파서 — docs/log-format.md 형식의 글자 한 줄을 CanFrame으로 바꾼다
 *
 *   "(0.100000) vcan0 100#1027000000000000"  ──parse_candump_line()──→  CanFrame + 시각(us)
 *
 * 여기서는 "글자 모양(문법)"만 검사한다. ID가 0x7FF 이하인지 같은 "값 검사"는
 * can_frame_is_valid()가, "그 메시지로 쓸 수 있는지"는 디코더가 맡는다 (docs/notes/17-log-format.md 6번).
 */
#ifndef LOG_PARSER_H
#define LOG_PARSER_H

#include <stdint.h>

#include "can_frame.h"

/* 파싱 결과 — 어떤 줄이 "왜" 실패했는지 로그로 남기기 위해 종류를 나눈다 */
typedef enum {
    PARSE_OK = 0,              /* 프레임 하나를 읽음 */
    PARSE_SKIP,                /* 빈 줄 / # 주석 → 오류 아님, 그냥 건너뜀 */
    PARSE_ERR_NULL,            /* line 또는 out이 NULL */
    PARSE_ERR_TIMESTAMP,       /* (초.마이크로초) 형식이 아님 */
    PARSE_ERR_INTERFACE,       /* 인터페이스 이름이 없거나 이상함 */
    PARSE_ERR_ID,              /* ID가 16진수 3자리가 아님 */
    PARSE_ERR_DATA,            /* 데이터가 16진수 짝수 자리(0~16)가 아님 */
    PARSE_ERR_UNSUPPORTED,     /* Extended ID / Remote Frame / CAN FD → 이 프로젝트는 지원 안 함 */
    PARSE_ERR_TRAILING         /* 줄 끝에 쓸데없는 글자가 남음 */
} ParseResult;

/* 한 줄을 파싱한다.
 * - line: '\0'으로 끝나는 문자열 (끝의 '\n', '\r\n'은 있어도 됨)
 * - out: PARSE_OK일 때만 채운다 (실패하면 건드리지 않음)
 * - ts_us: 타임스탬프를 마이크로초로 (NULL이면 저장 안 함). PARSE_OK일 때만 채운다
 *   (0.100000) → 100000, (1697800000.123456) → 1697800000123456
 *   float 대신 정수: 0.1초 같은 값을 오차 없이 담으려고 (M1-3) */
ParseResult parse_candump_line(const char *line, CanFrame *out, uint64_t *ts_us);

/* 결과 이름 (로그용) */
const char *parse_result_name(ParseResult r);

#endif
