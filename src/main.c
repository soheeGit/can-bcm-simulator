#include <stdio.h>

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

static void print_summary(const LogReadStats *stats, const VehicleInputs *in) {
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
    LogReadStats stats;
    vehicle_inputs_init(&in);

    if (log_read_file(opts.file, &in, &stats) != 0) {
        return EXIT_FAIL;
    }
    print_summary(&stats, &in);
    return EXIT_OK;
}
