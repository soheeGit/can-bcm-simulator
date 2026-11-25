/* 파일 출력 (M5-6): 프레임을 candump 형식 한 줄로 파일에 쓰는 CanSink 구현 (M4-8의 --out)
 *
 *   (0.100000) vcan0 400#0100000000000001
 */
#ifndef FILE_SINK_H
#define FILE_SINK_H

#include <stdio.h>

#include "can_sink.h"

typedef struct {
    FILE *out;               /* 이미 열린 파일. 닫는 건 연 쪽이 한다 */
    const char *iface;       /* 줄에 적을 인터페이스 이름 (기본 "vcan0") */
} FileSink;

void file_sink_init(FileSink *fs, FILE *out);

CanSink file_sink_as_can_sink(FileSink *fs);

#endif
