/* 로그 파일 읽기 — 파일 소스(file_source.h)를 프레임 루프(frame_loop.h)에 끼워서 돌린다
 *
 *   파일 ──FileSource──→ CanFrame + 시각 ──frame_loop_run()──→ 디스패처 → BCM → 0x400
 *
 * M3에서는 이 파일에 "줄 읽기 + 파싱 + 디스패치 + BCM"이 다 있었다.
 * M5-5에서 소켓도 같은 루프를 쓰도록 둘로 나눴다: 줄 읽기는 file_source.c, 나머지는 frame_loop.c.
 * 함수 이름과 인자는 그대로라 기존 코드와 테스트는 바뀌지 않는다.
 */
#ifndef LOG_READER_H
#define LOG_READER_H

#include <stdio.h>

#include "bcm.h"
#include "dispatcher.h"
#include "file_source.h"
#include "frame_loop.h"

/* 예전 이름 (M3 ~ M4). 같은 것이다 */
#define LOG_LINE_MAX FILE_SOURCE_LINE_MAX
#define LOG_MAX_TRANSITIONS FRAME_LOOP_MAX_TRANSITIONS
typedef FrameLoopStats LogReadStats;

/* 이미 열린 스트림에서 끝까지 읽는다 (테스트에서는 메모리 문자열을 스트림으로 넘길 수 있다)
 * 인자는 frame_loop_run()과 같다. stats의 lines / skipped / parse_errors도 채운다
 * 반환값: 0 = 끝까지 읽음, -1 = 인자가 NULL이거나 읽는 도중 입출력 오류 */
int log_read_stream(FILE *in, VehicleInputs *inputs, BcmContext *bcm, FILE *tx_out, LogReadStats *stats);

/* 파일을 열어서 log_read_stream()으로 읽고, 반드시 닫는다
 * 반환값: 0 = 성공, -1 = 파일을 열 수 없거나 읽기 오류 (이유를 ERROR 로그로 찍는다) */
int log_read_file(const char *path, VehicleInputs *inputs, BcmContext *bcm, FILE *tx_out, LogReadStats *stats);

#endif
