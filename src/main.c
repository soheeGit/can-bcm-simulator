#include <stdio.h>

#include "can_frame.h"
#include "signals.h"

int main(void) {
    printf("can-bcm-simulator v0.0.1\n");

    /* 확인용 샘플 (M3-6에서 로그 파일 처리로 바뀔 예정) */
    CanFrame speed = { .id = MSG_ID_VEHICLE_SPEED, .dlc = 8, .data = {0x10, 0x27} };
    printf("샘플 프레임: ");
    can_frame_print(&speed);

    VehicleSpeed s;
    char buf[32];
    if (decode_vehicle_speed(&speed, &s)) {
        vehicle_speed_format(&s, buf, sizeof(buf));
        printf("차속: %s\n", buf);
    }

    return 0;
}
