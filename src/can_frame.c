#include "can_frame.h"

#include <stdio.h>

bool can_frame_is_valid(const CanFrame *f) {
    if (f == NULL) {
        return false;
    }
    if (f->id > CAN_FRAME_STD_ID_MAX) {
        return false;
    }
    if (f->dlc > CAN_FRAME_MAX_DLC) {
        return false;
    }
    return true;
}

void can_frame_format(const CanFrame *f, char *buf, size_t size) {
    if (buf == NULL || size == 0u) {
        return;
    }
    if (f == NULL) {
        snprintf(buf, size, "(null)");
        return;
    }

    /* DLC가 잘못돼서 9 이상이어도 data[8] 밖을 읽지 않도록 개수를 제한한다 */
    uint8_t count = (f->dlc > CAN_FRAME_MAX_DLC) ? (uint8_t)CAN_FRAME_MAX_DLC : f->dlc;

    int written = snprintf(buf, size, "0x%03X [%u]", (unsigned)f->id, (unsigned)f->dlc);
    for (uint8_t i = 0; i < count; i++) {
        if (written < 0 || (size_t)written >= size) {
            return;                                     /* 버퍼가 꽉 참 */
        }
        written += snprintf(buf + written, size - (size_t)written, " %02X", f->data[i]);
    }
}

void can_frame_print(const CanFrame *f) {
    char buf[CAN_FRAME_STR_SIZE];
    can_frame_format(f, buf, sizeof(buf));
    printf("%s\n", buf);
}
