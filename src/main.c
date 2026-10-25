#include <stdio.h>

#include "can_frame.h"

int main(void) {
    printf("can-bcm-simulator v0.0.1\n");

    /* M1-2 확인용: 0x100 VEHICLE_SPEED 프레임 (100.00 km/h) */
    CanFrame speed = { .id = 0x100, .dlc = 8, .data = {0x10, 0x27} };
    printf("샘플 프레임: ");
    can_frame_print(&speed);
    printf("유효한 프레임? %s\n", can_frame_is_valid(&speed) ? "yes" : "no");

    return 0;
}
