#include "cli.h"

#include <getopt.h>
#include <stddef.h>

/* getopt_long은 "어디까지 읽었는지"를 전역 변수(optind)에 기억한다.
 * 한 프로그램에서 여러 번 해석하려면(테스트) 처음으로 되돌려야 하는데, 방법이 플랫폼마다 다르다 */
static void reset_getopt(void) {
#if defined(__GLIBC__)
    optind = 0;              /* 리눅스(glibc): 0이면 완전히 처음부터 */
#else
    optreset = 1;            /* 맥/BSD: optreset으로 되돌린다 */
    optind = 1;
#endif
}

void cli_print_usage(FILE *out, const char *prog) {
    fprintf(out,
            "사용법: %s --file <로그 파일> [--log-level <레벨>]\n"
            "\n"
            "옵션:\n"
            "  -f, --file <경로>         읽을 candump 로그 파일 (필수)\n"
            "  -l, --log-level <레벨>    debug | info | warn | error | off (기본: info)\n"
            "  -h, --help                이 도움말\n"
            "  -V, --version             버전\n"
            "\n"
            "예:\n"
            "  %s --file data/scenarios/02_door_open_while_driving.log\n"
            "  %s -f data/scenarios/04_invalid_frames.log -l warn\n",
            prog, prog, prog);
}

CliResult cli_parse(int argc, char *argv[], CliOptions *opts, FILE *err) {
    if (argv == NULL || opts == NULL) {
        return CLI_ERROR;
    }
    opts->file = NULL;
    opts->log_level = LOG_LEVEL_INFO;

    /* 긴 옵션 표: { 이름, 값이 필요한가, (안 씀), 짧은 옵션 글자 } */
    static const struct option long_opts[] = {
        { "file",      required_argument, NULL, 'f' },
        { "log-level", required_argument, NULL, 'l' },
        { "help",      no_argument,       NULL, 'h' },
        { "version",   no_argument,       NULL, 'V' },
        { NULL, 0, NULL, 0 }                              /* 표의 끝 표시 */
    };
    /* 짧은 옵션: 글자 뒤의 ':'는 "값이 필요하다"는 뜻 (-f 경로, -l 레벨) */
    static const char short_opts[] = "f:l:hV";

    reset_getopt();
    opterr = 0;                  /* getopt가 직접 오류를 찍지 않게 → 우리가 한국어로 찍는다 */

    int c;
    while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
        switch (c) {
            case 'f':
                opts->file = optarg;                      /* optarg = 옵션 뒤의 값 */
                break;
            case 'l':
                if (!log_parse_level(optarg, &opts->log_level)) {
                    if (err != NULL) {
                        fprintf(err, "알 수 없는 로그 레벨: %s (debug|info|warn|error|off)\n", optarg);
                    }
                    return CLI_ERROR;
                }
                break;
            case 'h':
                return CLI_HELP;
            case 'V':
                return CLI_VERSION;
            case '?':
            default:
                /* 모르는 옵션이거나, 값이 필요한 옵션(-f)에 값이 없음 */
                if (err != NULL) {
                    if (optopt != 0) {
                        fprintf(err, "잘못된 옵션 또는 값이 없음: -%c\n", optopt);
                    } else {
                        fprintf(err, "알 수 없는 옵션: %s\n", argv[optind - 1]);
                    }
                }
                return CLI_ERROR;
        }
    }

    /* 옵션이 아닌 인자가 남았다 (예: --file 없이 경로만 씀) */
    if (optind < argc) {
        if (err != NULL) {
            fprintf(err, "알 수 없는 인자: %s (로그 파일은 --file로 지정)\n", argv[optind]);
        }
        return CLI_ERROR;
    }
    if (opts->file == NULL) {
        if (err != NULL) {
            fprintf(err, "--file이 필요합니다\n");
        }
        return CLI_ERROR;
    }
    return CLI_OK;
}
