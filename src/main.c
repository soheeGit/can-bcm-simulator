#include <stdio.h>

#include "dispatcher.h"
#include "log_reader.h"
#include "logger.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    /* M3-5: ./bcm_sim <로그 파일> [로그 레벨]  (M3-6에서 --file, --log-level 옵션으로 바뀔 예정) */
    if (argc < 2) {
        fprintf(stderr, "사용법: %s <로그 파일> [debug|info|warn|error]\n", argv[0]);
        fprintf(stderr, "예:     %s data/scenarios/01_normal_drive.log info\n", argv[0]);
        return 1;
    }
    if (argc >= 3) {
        LogLevel level;
        if (!log_parse_level(argv[2], &level)) {
            fprintf(stderr, "알 수 없는 로그 레벨: %s (debug|info|warn|error|off)\n", argv[2]);
            return 1;
        }
        log_set_level(level);
    }

    LOG_INFO("can-bcm-simulator v0.0.1 시작: %s (로그 레벨 %s)", argv[1], log_level_name(log_get_level()));

    VehicleInputs in;
    LogReadStats stats;
    vehicle_inputs_init(&in);

    if (log_read_file(argv[1], &in, &stats) != 0) {
        return 1;
    }

    /* 결과 요약은 로그가 아니라 프로그램의 "출력"이라 레벨과 상관없이 찍는다 */
    char speed_str[32], door_str[64], ign_str[32];
    vehicle_speed_format(&in.speed, speed_str, sizeof(speed_str));
    door_status_format(&in.doors, door_str, sizeof(door_str));
    ign_status_format(&in.ign, ign_str, sizeof(ign_str));

    printf("\n결과\n");
    printf("  read %u, ok %u, error %u  (빈 줄/주석 %u)\n",
           stats.lines, stats.frames, stats.parse_errors, stats.skipped);
    printf("  디스패처: ok=%u ignored=%u error=%u\n",
           stats.dispatch.ok, stats.dispatch.ignored, stats.dispatch.error);
    printf("\n최종 차량 상태\n");
    printf("  차속 : %s\n", speed_str);
    printf("  도어 : %s\n", door_str);
    printf("  시동 : %s\n", ign_str);

    return 0;
}
