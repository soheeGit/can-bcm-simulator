#include "log_parser.h"

#include <stdbool.h>
#include <stddef.h>

#define TS_MAX_SEC_DIGITS   12u   /* 초 부분 최대 자리 수 (overflow 방지, 실제 candump는 10자리) */
#define TS_FRAC_DIGITS      6u    /* 마이크로초 = 소수점 아래 6자리 */
#define IFNAME_MAX_LEN      15u   /* Linux 인터페이스 이름 최대 길이 (IFNAMSIZ 16 - '\0') */
#define STD_ID_HEX_DIGITS   3u    /* Standard ID: 16진수 3자리 */
#define EXT_ID_HEX_DIGITS   8u    /* Extended ID: 16진수 8자리 (지원 안 함) */
#define DATA_MAX_HEX_DIGITS (CAN_FRAME_MAX_DLC * 2u)   /* 16 */

/* ---------- 글자 하나 검사 도우미 ---------- */

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

/* 16진수 글자면 0~15, 아니면 -1 */
static int hex_value(char c) {
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    return -1;
}

static bool is_ifname_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_digit(c) || c == '_' || c == '-';
}

/* 줄 끝 공백 (space, tab, \r, \n) */
static bool is_trailing_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/* 빈 줄 또는 # 주석인가 */
static bool is_skip_line(const char *p) {
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return *p == '\0' || *p == '\r' || *p == '\n' || *p == '#';
}

/* ---------- 필드별 파싱 (성공하면 p를 다음 위치로 옮긴다) ---------- */

/* "(초.마이크로초)" → us. 소수점 아래는 1~6자리 허용, 모자라면 뒤에 0을 채운 것으로 본다 (0.1 → 100000) */
static bool parse_timestamp(const char **pp, uint64_t *us) {
    const char *p = *pp;
    if (*p != '(') {
        return false;
    }
    p++;

    uint64_t sec = 0u;
    uint32_t sec_digits = 0u;
    while (is_digit(*p)) {
        if (sec_digits == TS_MAX_SEC_DIGITS) {
            return false;                       /* 너무 큰 숫자 → overflow 방지 */
        }
        sec = sec * 10u + (uint64_t)(*p - '0');
        sec_digits++;
        p++;
    }
    if (sec_digits == 0u) {
        return false;                           /* "(.5)" 처럼 정수 부분이 없음 */
    }

    uint64_t frac = 0u;
    uint32_t frac_digits = 0u;
    if (*p == '.') {
        p++;
        while (is_digit(*p)) {
            if (frac_digits == TS_FRAC_DIGITS) {
                return false;                   /* 마이크로초보다 정밀한 값은 형식 위반 */
            }
            frac = frac * 10u + (uint64_t)(*p - '0');
            frac_digits++;
            p++;
        }
        if (frac_digits == 0u) {
            return false;                       /* "(0.)" 처럼 점 뒤가 비어 있음 */
        }
    }
    while (frac_digits < TS_FRAC_DIGITS) {      /* "0.1" → 0.100000 */
        frac *= 10u;
        frac_digits++;
    }

    if (*p != ')') {
        return false;
    }
    p++;

    *us = sec * 1000000u + frac;
    *pp = p;
    return true;
}

/* 인터페이스 이름 1~15자 */
static bool parse_interface(const char **pp) {
    const char *p = *pp;
    uint32_t len = 0u;
    while (is_ifname_char(*p)) {
        if (len == IFNAME_MAX_LEN) {
            return false;
        }
        len++;
        p++;
    }
    if (len == 0u) {
        return false;
    }
    *pp = p;
    return true;
}

/* "ID#" 까지. 3자리면 OK, 8자리면 Extended(지원 안 함), 그 외는 형식 오류 */
static ParseResult parse_id(const char **pp, uint32_t *id) {
    const char *p = *pp;
    uint32_t value = 0u;
    uint32_t digits = 0u;
    while (*p != '#' && *p != '\0') {
        int v = hex_value(*p);
        if (v < 0 || digits == EXT_ID_HEX_DIGITS) {
            return PARSE_ERR_ID;                /* 16진수가 아닌 글자, 또는 너무 김 */
        }
        value = (value << 4) | (uint32_t)v;
        digits++;
        p++;
    }
    if (*p != '#') {
        return PARSE_ERR_ID;                    /* '#'이 없음 */
    }
    if (digits == EXT_ID_HEX_DIGITS) {
        return PARSE_ERR_UNSUPPORTED;           /* Extended ID */
    }
    if (digits != STD_ID_HEX_DIGITS) {
        return PARSE_ERR_ID;
    }
    *id = value;
    *pp = p + 1;                                /* '#' 다음으로 */
    return PARSE_OK;
}

/* 데이터 16진수 0~16자리(짝수) → 바이트 배열 */
static ParseResult parse_data(const char **pp, uint8_t data[CAN_FRAME_MAX_DLC], uint8_t *dlc) {
    const char *p = *pp;
    if (*p == '#') {
        return PARSE_ERR_UNSUPPORTED;           /* "##" = CAN FD */
    }
    if (*p == 'R' || *p == 'r') {
        return PARSE_ERR_UNSUPPORTED;           /* Remote Frame */
    }

    uint32_t digits = 0u;
    while (*p != '\0' && !is_trailing_space(*p)) {
        int v = hex_value(*p);
        if (v < 0 || digits == DATA_MAX_HEX_DIGITS) {
            return PARSE_ERR_DATA;              /* 16진수 아님, 또는 16자리 초과 */
        }
        uint32_t byte_index = digits / 2u;
        if (digits % 2u == 0u) {
            data[byte_index] = (uint8_t)((uint32_t)v << 4);    /* 앞 글자 = 윗 4비트 */
        } else {
            data[byte_index] |= (uint8_t)v;                    /* 뒤 글자 = 아랫 4비트 */
        }
        digits++;
        p++;
    }
    if (digits % 2u != 0u) {
        return PARSE_ERR_DATA;                  /* 홀수 자리 */
    }
    *dlc = (uint8_t)(digits / 2u);
    *pp = p;
    return PARSE_OK;
}

/* ---------- 한 줄 파싱 ---------- */

ParseResult parse_candump_line(const char *line, CanFrame *out, uint64_t *ts_us) {
    if (line == NULL || out == NULL) {
        return PARSE_ERR_NULL;
    }
    if (is_skip_line(line)) {
        return PARSE_SKIP;
    }

    const char *p = line;
    uint64_t ts = 0u;
    CanFrame f = {0};                           /* 임시 변수: 성공했을 때만 out에 반영 (M2-6) */

    /* 1) (타임스탬프) */
    if (!parse_timestamp(&p, &ts)) {
        return PARSE_ERR_TIMESTAMP;
    }
    /* 2) 공백 한 칸 + 인터페이스 */
    if (*p != ' ') {
        return PARSE_ERR_INTERFACE;
    }
    p++;
    if (!parse_interface(&p)) {
        return PARSE_ERR_INTERFACE;
    }
    /* 3) 공백 한 칸 + ID#
     *    인터페이스 이름 뒤에 공백이 아닌 글자가 오면 인터페이스 필드가 깨진 것
     *    (예: "(0.1) 100#..." → "100"을 인터페이스로 읽었는데 뒤에 '#'이 옴) */
    if (*p != ' ') {
        return PARSE_ERR_INTERFACE;
    }
    p++;
    ParseResult r = parse_id(&p, &f.id);
    if (r != PARSE_OK) {
        return r;
    }
    /* 4) 데이터 */
    r = parse_data(&p, f.data, &f.dlc);
    if (r != PARSE_OK) {
        return r;
    }
    /* 5) 줄 끝: 공백/줄바꿈만 남아야 한다 */
    while (*p != '\0') {
        if (!is_trailing_space(*p)) {
            return PARSE_ERR_TRAILING;
        }
        p++;
    }

    *out = f;
    if (ts_us != NULL) {
        *ts_us = ts;
    }
    return PARSE_OK;
}

const char *parse_result_name(ParseResult r) {
    switch (r) {
        case PARSE_OK:              return "OK";
        case PARSE_SKIP:            return "SKIP";
        case PARSE_ERR_NULL:        return "ERR_NULL";
        case PARSE_ERR_TIMESTAMP:   return "ERR_TIMESTAMP";
        case PARSE_ERR_INTERFACE:   return "ERR_INTERFACE";
        case PARSE_ERR_ID:          return "ERR_ID";
        case PARSE_ERR_DATA:        return "ERR_DATA";
        case PARSE_ERR_UNSUPPORTED: return "ERR_UNSUPPORTED";
        case PARSE_ERR_TRAILING:    return "ERR_TRAILING";
        default:                    return "UNKNOWN";
    }
}
