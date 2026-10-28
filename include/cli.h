/* 명령줄 인자(CLI) 해석
 *
 *   ./bcm_sim --file data/scenarios/01_normal_drive.log --log-level debug
 *   ./bcm_sim -f data/scenarios/01_normal_drive.log -l debug        (짧은 옵션)
 *
 * main()에서 바로 해석하지 않고 따로 뺀 이유: 테스트에서 argv를 만들어 넣어 볼 수 있게
 */
#ifndef CLI_H
#define CLI_H

#include <stdio.h>

#include "logger.h"

typedef struct {
    const char *file;        /* --file: 읽을 로그 파일 (필수) */
    LogLevel    log_level;   /* --log-level: 기본 INFO */
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
