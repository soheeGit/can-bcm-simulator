#include <stdio.h>

#include "can_frame.h"
#include "dispatcher.h"
#include "signals.h"

int main(void) {
    printf("can-bcm-simulator v0.0.1\n\n");

    /* 확인용 샘플 시나리오 (M3에서 로그 파일 처리로 바뀔 예정) */
    static const struct { uint32_t t; CanFrame f; } samples[] = {
        {   0, { .id = MSG_ID_IGN_STATUS,    .dlc = 8, .data = {0x02, 0x03} } },   /* ON, D */
        { 100, { .id = MSG_ID_VEHICLE_SPEED, .dlc = 8, .data = {0x10, 0x27} } },   /* 100.00 km/h */
        { 150, { .id = 0x555,                .dlc = 8, .data = {0xAA} } },         /* 관심 없음 */
        { 200, { .id = MSG_ID_DOOR_STATUS,   .dlc = 8, .data = {0x01} } },         /* 운전석 열림 */
        { 250, { .id = MSG_ID_VEHICLE_SPEED, .dlc = 1, .data = {0x00} } },         /* 깨진 프레임 */
    };

    VehicleInputs in;
    DispatchStats stats = {0};
    vehicle_inputs_init(&in);

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        char frame_str[CAN_FRAME_STR_SIZE];
        can_frame_format(&samples[i].f, frame_str, sizeof(frame_str));
        DispatchResult r = can_dispatch(&samples[i].f, &in, samples[i].t, &stats);
        printf("[%4u ms] RX %-36s → %s\n", samples[i].t, frame_str, dispatch_result_name(r));
    }

    char speed_str[32], door_str[64], ign_str[32];
    vehicle_speed_format(&in.speed, speed_str, sizeof(speed_str));
    door_status_format(&in.doors, door_str, sizeof(door_str));
    ign_status_format(&in.ign, ign_str, sizeof(ign_str));

    printf("\n현재 차량 상태\n");
    printf("  차속  : %s\n", speed_str);
    printf("  도어  : %s\n", door_str);
    printf("  시동  : %s\n", ign_str);
    printf("  통계  : ok=%u ignored=%u error=%u\n", stats.ok, stats.ignored, stats.error);

    return 0;
}
