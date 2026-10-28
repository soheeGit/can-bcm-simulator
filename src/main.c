#include <stdio.h>

#include "dispatcher.h"
#include "log_reader.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    printf("can-bcm-simulator v0.0.1\n");

    /* M3-3: 첫 번째 인자를 로그 파일 경로로 받는다 (M3-6에서 --file 옵션으로 바뀔 예정) */
    if (argc < 2) {
        fprintf(stderr, "사용법: %s <로그 파일>\n", argv[0]);
        fprintf(stderr, "예:     %s data/examples/mixed.log\n", argv[0]);
        return 1;
    }

    VehicleInputs in;
    LogReadStats stats;
    vehicle_inputs_init(&in);

    printf("파일: %s\n\n", argv[1]);
    fflush(stdout);       /* stdout은 모아서 내보내고 stderr는 바로 나가서, 순서가 섞이지 않게 먼저 비운다 */
    if (log_read_file(argv[1], &in, &stats, stderr) != 0) {
        return 1;
    }

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
