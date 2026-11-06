/* SocketCAN 소켓 열기 테스트 (M5-3, Linux 전용)
 *
 * vcan0이 있어야 하는 테스트는, vcan0이 없으면 건너뛴다 (VM에서 ./scripts/setup_vcan.sh 먼저)
 * 다른 테스트들은 /dev/null로 stderr를 버리지 않으니, 실패 케이스의 perror 메시지가 화면에 같이 보인다 (정상)
 */
#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>

#include <linux/can.h>
#include <sys/socket.h>

#include "can_socket.h"
#include "test_framework.h"

static int vcan0_exists(void) {
    return if_nametoindex("vcan0") != 0u;
}

static void test_open_vcan0(void) {
    if (!vcan0_exists()) {
        printf("    (vcan0 없음: 건너뜀. ./scripts/setup_vcan.sh)\n");
        return;
    }
    int fd = can_socket_open("vcan0");
    ASSERT_TRUE(fd >= 0);                          /* 0, 1, 2는 stdin/out/err라 보통 3부터 */

    /* 정말 vcan0에 묶였나: 소켓에게 "너 어디에 묶여 있니?"를 물어본다 (bind 안 했으면 장치 번호가 0) */
    struct sockaddr_can addr;
    socklen_t len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    ASSERT_EQ(0, getsockname(fd, (struct sockaddr *)&addr, &len));
    ASSERT_EQ(AF_CAN, addr.can_family);
    ASSERT_EQ((int)if_nametoindex("vcan0"), addr.can_ifindex);

    int fd2 = can_socket_open("vcan0");            /* 같은 장치에 소켓을 하나 더 (candump 여러 개처럼) */
    ASSERT_TRUE(fd2 >= 0);
    ASSERT_TRUE(fd2 != fd);                        /* 다른 번호 */

    can_socket_close(fd);
    can_socket_close(fd2);
}

static void test_open_unknown_iface(void) {
    printf("    (아래 ioctl 에러 메시지는 일부러 낸 것)\n");
    errno = 0;
    ASSERT_EQ(-1, can_socket_open("nope0"));
    ASSERT_EQ(ENODEV, errno);                      /* No such device */
}

static void test_open_bad_name(void) {
    printf("    (아래 인터페이스 이름 에러 메시지 3개는 일부러 낸 것)\n");
    errno = 0;
    ASSERT_EQ(-1, can_socket_open(NULL));
    ASSERT_EQ(EINVAL, errno);
    errno = 0;
    ASSERT_EQ(-1, can_socket_open(""));
    ASSERT_EQ(EINVAL, errno);
    errno = 0;
    ASSERT_EQ(-1, can_socket_open("this_name_is_way_too_long"));   /* 16자 이상 */
    ASSERT_EQ(EINVAL, errno);
}

static void test_close_negative_is_safe(void) {
    can_socket_close(-1);                          /* 죽지 않기 */
    ASSERT_TRUE(1);
}

int main(void) {
    printf("[test_can_socket]\n");
    RUN_TEST(test_open_vcan0);
    RUN_TEST(test_open_unknown_iface);
    RUN_TEST(test_open_bad_name);
    RUN_TEST(test_close_negative_is_safe);
    return TEST_SUMMARY();
}
