/* 파일 소스 (M5-5): candump 로그 파일을 한 줄씩 읽어 CanFrame으로 주는 CanSource 구현
 *
 *   FILE ──fgets──→ 한 줄 ──parse_candump_line()──→ CanFrame + 시각(첫 프레임 = 0ms)
 *
 * M3의 log_reader.c에 있던 "줄 읽기 + 파싱" 부분을 떼어 낸 것.
 * 빈 줄/주석은 건너뛰고, 형식이 틀린 줄은 줄 번호와 이유를 WARN으로 찍고 건너뛴다 (멈추지 않는다).
 */
#ifndef FILE_SOURCE_H
#define FILE_SOURCE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "can_source.h"

/* 한 줄을 담을 버퍼 크기. 정상 줄은 최대 60자 정도라 넉넉하게 */
#define FILE_SOURCE_LINE_MAX 128u

typedef struct {
    FILE *in;                /* 이미 열린 파일. 닫는 건 연 쪽이 한다 */
    uint32_t lines;          /* 읽은 줄 수 (빈 줄, 주석 포함) */
    uint32_t frames;         /* 파싱에 성공한 프레임 수 */
    uint32_t skipped;        /* 빈 줄 / 주석 */
    uint32_t parse_errors;   /* 형식이 틀린 줄 (너무 긴 줄 포함) */
    bool     have_first_ts;
    uint64_t first_ts_us;    /* 첫 프레임의 시각. 이걸 빼서 0ms부터 시작 */
} FileSource;

void file_source_init(FileSource *fs, FILE *in);

/* fs를 CanSource 인터페이스로 감싼다 (Java: CanSource src = fileSource;) */
CanSource file_source_as_can_source(FileSource *fs);

#endif
