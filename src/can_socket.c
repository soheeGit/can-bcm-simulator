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
