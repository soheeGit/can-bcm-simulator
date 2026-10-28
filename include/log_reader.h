/* 로그 파일 읽기 — 파일을 한 줄씩 읽어 파서(M3-2) → 디스패처(M2-6)로 흘려보낸다
 *
 *   파일 ──fgets──→ 한 줄 ──parse_candump_line()──→ CanFrame + 시각 ──can_dispatch()──→ VehicleInputs
 *
 * 잘못된 줄은 줄 번호와 이유를 WARN 로그로 찍고 다음 줄로 넘어간다 (멈추지 않는다).
 * 값이 바뀌면 INFO, 모든 프레임은 DEBUG 로그로 찍는다 (logger.h, M3-5).
 */
#ifndef LOG_READER_H
#define LOG_READER_H

#include <stdint.h>
#include <stdio.h>

#include "dispatcher.h"

/* 한 줄을 담을 버퍼 크기. 정상 줄은 최대 60자 정도라 넉넉하게 */
#define LOG_LINE_MAX 128u

typedef struct {
    uint32_t lines;          /* 읽은 줄 수 (빈 줄, 주석 포함) */
    uint32_t frames;         /* 파싱에 성공한 프레임 수 */
    uint32_t skipped;        /* 빈 줄 / 주석 */
    uint32_t parse_errors;   /* 형식이 틀린 줄 (너무 긴 줄 포함) */
    DispatchStats dispatch;  /* 파싱된 프레임의 디스패처 결과 (ok / ignored / error) */
} LogReadStats;

/* 이미 열린 스트림에서 끝까지 읽는다 (테스트에서는 메모리 문자열을 스트림으로 넘길 수 있다)
 * - inputs: 프레임을 반영할 차량 상태 (미리 vehicle_inputs_init 해 둘 것)
 * - stats: 결과 통계 (0으로 초기화해서 채운다)
 * 반환값: 0 = 끝까지 읽음, -1 = 인자가 NULL이거나 읽는 도중 입출력 오류 */
int log_read_stream(FILE *in, VehicleInputs *inputs, LogReadStats *stats);

/* 파일을 열어서 log_read_stream()으로 읽고, 반드시 닫는다
 * 반환값: 0 = 성공, -1 = 파일을 열 수 없거나 읽기 오류 (이유를 ERROR 로그로 찍는다) */
int log_read_file(const char *path, VehicleInputs *inputs, LogReadStats *stats);

#endif
