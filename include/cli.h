/* 명령줄 인자(CLI) 해석
 *
 *   ./bcm_sim --file data/scenarios/01_normal_drive.log --log-level debug
 *   ./bcm_sim -f data/scenarios/01_normal_drive.log -l debug        (짧은 옵션)
 *   ./bcm_sim -f data/scenarios/02_door_open_while_driving.log -o result.log   (0x400 송신 기록, M4-8)
 *   ./bcm_sim --iface vcan0                                                   (실시간 BCM, M5-5, Linux)
 *   ./bcm_sim --iface vcan0 --dump                                            (실시간 수신 덤프, M5-4, Linux)
 *
 * main()에서 바로 해석하지 않고 따로 뺀 이유: 테스트에서 argv를 만들어 넣어 볼 수 있게
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdio.h>

#include "logger.h"

typedef struct {
    const char *file;        /* --file: 읽을 로그 파일 */
    const char *iface;       /* --iface: CAN 장치 이름(예: vcan0). --file과 둘 중 하나만 (M5-4, M5-5) */
    bool        no_filter;   /* --no-filter: 수신 필터를 걸지 않고 전부 받는다 (--iface와 같이, M5-7) */
    bool        dump;        /* --dump: 받는 프레임을 해석해서 찍기만 한다 (--iface와 같이) */
    LogLevel    log_level;   /* --log-level: 기본 INFO */
    const char *out_file;    /* --out: BCM이 보내는 0x400을 candump 형식으로 쓸 파일. 없으면 NULL (M4-8) */
} CliOptions;

typedef enum {
    CLI_OK = 0,      /* 옵션을 다 읽음 → 프로그램 실행 */
    CLI_HELP,        /* --help → 사용법 출력하고 종료 (성공) */
    CLI_VERSION,     /* --version → 버전 출력하고 종료 (성공) */
    CLI_ERROR        /* 잘못된 옵션 → 이유를 찍고 종료 (실패) */
} CliResult;

/* argv를 해석해 opts를 채운다. CLI_ERROR면 이유를 err에 찍는다 (NULL이면 안 찍음) */
CliResult cli_parse(int argc, char *argv[], CliOptions *opts, FILE *err);

/* 사용법 출력 (--help) */
void cli_print_usage(FILE *out, const char *prog);

#endif
