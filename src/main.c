#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "bcm.h"
#include "cli.h"
#include "dispatcher.h"
#include "log_reader.h"
#include "logger.h"
#include "signals.h"
#include "version.h"

/* 종료 코드: 0 = 성공, 1 = 실행 중 실패(파일을 못 엶 등), 2 = 사용법이 틀림 */
#define EXIT_OK     0
#define EXIT_FAIL   1
#define EXIT_USAGE  2

static void print_summary(const LogReadStats *stats, const VehicleInputs *in, const BcmContext *bcm) {
    char speed_str[32], door_str[64], ign_str[32];
    vehicle_speed_format(&in->speed, speed_str, sizeof(speed_str));
    door_status_format(&in->doors, door_str, sizeof(door_str));
    ign_status_format(&in->ign, ign_str, sizeof(ign_str));

    printf("\n결과\n");
    printf("  read %u, ok %u, error %u  (빈 줄/주석 %u)\n",
           stats->lines, stats->frames, stats->parse_errors, stats->skipped);
    printf("  디스패처: ok=%u ignored=%u error=%u\n",
           stats->dispatch.ok, stats->dispatch.ignored, stats->dispatch.error);
    printf("\n최종 차량 상태\n");
    printf("  차속 : %s\n", speed_str);
    printf("  도어 : %s\n", door_str);
    printf("  시동 : %s\n", ign_str);
    printf("\nBCM\n");
    printf("  최종 상태 : %s\n", bcm_state_name(bcm->state));
    printf("  상태 전이 : %u회, 도어 잠금 명령 : %u회\n", stats->bcm_transitions, stats->bcm_lock_cmds);
    printf("  0x400 송신 : %u프레임\n", stats->tx_frames);
}

int main(int argc, char *argv[]) {
    CliOptions opts;
    switch (cli_parse(argc, argv, &opts, stderr)) {
        case CLI_OK:
            break;
        case CLI_HELP:
            cli_print_usage(stdout, argv[0]);
            return EXIT_OK;
        case CLI_VERSION:
            printf("can-bcm-simulator %s\n", BCM_SIM_VERSION);
            return EXIT_OK;
        case CLI_ERROR:
        default:
            fprintf(stderr, "도움말: %s --help\n", argv[0]);
            return EXIT_USAGE;
    }

    log_set_level(opts.log_level);
    LOG_INFO("can-bcm-simulator %s 시작: %s (로그 레벨 %s)",
             BCM_SIM_VERSION, opts.file, log_level_name(opts.log_level));

    VehicleInputs in;
    BcmContext bcm;
    LogReadStats stats;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);                         /* 처음 한 번 (프레임마다 부르면 상태를 잊는다) */

    FILE *tx_out = NULL;                    /* --out (M4-8) */
    if (opts.out_file != NULL) {
        tx_out = fopen(opts.out_file, "w");
        if (tx_out == NULL) {
            LOG_ERROR("출력 파일을 열 수 없습니다: %s (%s)", opts.out_file, strerror(errno));
            return EXIT_FAIL;
        }
    }

    int result = log_read_file(opts.file, &in, &bcm, tx_out, &stats);
    if (tx_out != NULL) {
        fclose(tx_out);
    }
    if (result != 0) {
        return EXIT_FAIL;
    }
    print_summary(&stats, &in, &bcm);
    if (opts.out_file != NULL) {
        printf("  → %s\n", opts.out_file);
    }
    return EXIT_OK;
}
