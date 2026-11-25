/* struct ifreq, IFNAMSIZ 같은 POSIX 밖의 이름을 -std=c11에서도 쓰려면 include 전에 켜야 한다 */
#define _DEFAULT_SOURCE

#include "can_socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

int can_socket_open(const char *ifname) {
    if (ifname == NULL || ifname[0] == '\0' || strlen(ifname) >= IFNAMSIZ) {
        errno = EINVAL;
        perror("can_socket_open: 인터페이스 이름");
        return -1;
    }

    /* 1) 소켓 만들기: CAN 주소 체계(PF_CAN), 프레임 단위 그대로(SOCK_RAW), 기본 CAN 프로토콜(CAN_RAW) */
    int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        perror("socket(PF_CAN)");
        return -1;
    }

    /* 2) 이름("vcan0") → 커널이 붙인 장치 번호 (ip link의 "6: vcan0"의 6) */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1u);
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        int saved = errno;                 /* perror/close가 errno를 바꿀 수 있어서 보관 */
        perror("ioctl(SIOCGIFINDEX)");
        close(fd);
        errno = saved;
        return -1;
    }

    /* 3) 그 장치에 묶기 (TCP의 bind/connect처럼 "어디와 통신할지" 정하기) */
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        int saved = errno;                 /* perror/close가 errno를 바꿀 수 있어서 보관 */
        perror("bind(AF_CAN)");
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}

void can_socket_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

/* ---------- 받기 (M5-4) ---------- */

bool can_frame_from_linux(const struct can_frame *in, CanFrame *out) {
    if (in == NULL || out == NULL) {
        return false;
    }
    /* can_id의 위쪽 3비트는 ID가 아니라 "표시"(flag)다: EFF(29비트) / RTR / ERR */
    if ((in->can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0u) {
        return false;
    }
    if (in->len > CAN_FRAME_MAX_DLC) {
        return false;
    }
    out->id = in->can_id & CAN_SFF_MASK;
    out->dlc = in->len;
    memset(out->data, 0, CAN_FRAME_MAX_DLC);
    memcpy(out->data, in->data, in->len);
    return true;
}

CanReadResult can_socket_read(int fd, CanFrame *out) {
    struct can_frame raw;
    ssize_t n = read(fd, &raw, sizeof(raw));
    if (n < 0) {
        perror("read(CAN)");
        return CAN_READ_ERROR;
    }
    if ((size_t)n != sizeof(raw)) {            /* CAN_RAW는 프레임 하나를 통째로 준다. 모자라면 이상한 것 */
        fprintf(stderr, "read(CAN): %zd바이트 (기대 %zu)\n", n, sizeof(raw));
        return CAN_READ_ERROR;
    }
    return can_frame_from_linux(&raw, out) ? CAN_READ_OK : CAN_READ_SKIP;
}

/* ---------- 보내기 (M5-6) ---------- */

bool can_frame_to_linux(const CanFrame *in, struct can_frame *out) {
    if (in == NULL || out == NULL) {
        return false;
    }
    if (in->id > CAN_FRAME_STD_ID_MAX || in->dlc > CAN_FRAME_MAX_DLC) {
        return false;
    }
    memset(out, 0, sizeof(*out));                   /* __pad, __res0 같은 예약 칸까지 0으로 */
    out->can_id = in->id;                           /* 표시(flag) 비트는 0 = 일반 11비트 데이터 프레임 */
    out->len = in->dlc;
    memcpy(out->data, in->data, in->dlc);
    return true;
}

int can_socket_write(int fd, const CanFrame *f) {
    struct can_frame raw;
    if (!can_frame_to_linux(f, &raw)) {
        errno = EINVAL;
        perror("can_socket_write: 프레임");
        return -1;
    }
    ssize_t n = write(fd, &raw, sizeof(raw));
    if (n < 0) {
        perror("write(CAN)");
        return -1;
    }
    if ((size_t)n != sizeof(raw)) {                 /* CAN_RAW는 프레임 하나를 통째로 보낸다. 모자라면 이상한 것 */
        fprintf(stderr, "write(CAN): %zd바이트 (기대 %zu)\n", n, sizeof(raw));
        return -1;
    }
    return 0;
}

/* ---------- 수신 필터 (M5-7) ---------- */

void can_filter_exact(struct can_filter *f, uint32_t id) {
    f->can_id = id;
    /* 비교할 비트: ID 11비트 전부 + EFF + RTR 표시.
     * 표시 비트까지 비교해야 "29비트 ID 0x100"이나 "Remote 0x100"이 섞여 들어오지 않는다 (둘 다 0이어야 통과) */
    f->can_mask = CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG;
}

int can_socket_set_filter(int fd, const uint32_t *ids, size_t count) {
    if ((ids == NULL && count > 0u) || count > CAN_SOCKET_MAX_FILTERS) {
        errno = EINVAL;
        perror("can_socket_set_filter: 필터 목록");
        return -1;
    }
    struct can_filter filters[CAN_SOCKET_MAX_FILTERS];
    for (size_t i = 0; i < count; i++) {
        can_filter_exact(&filters[i], ids[i]);
    }
    /* 소켓 옵션으로 커널에 필터 목록을 넘긴다. 0개면 (NULL, 0) = 아무것도 안 받음 */
    if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FILTER, count > 0u ? filters : NULL,
                   (socklen_t)(count * sizeof(struct can_filter))) < 0) {
        perror("setsockopt(CAN_RAW_FILTER)");
        return -1;
    }
    return 0;
}
