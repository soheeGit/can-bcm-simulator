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
#include <unistd.h>

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

/* ---------- 받기 (M5-4) ---------- */

static struct can_frame linux_frame(canid_t id, uint8_t len, uint8_t b0, uint8_t b1) {
    struct can_frame f;
    memset(&f, 0, sizeof(f));
    f.can_id = id;
    f.len = len;
    f.data[0] = b0;
    f.data[1] = b1;
    return f;
}

static void test_from_linux_standard(void) {
    struct can_frame in = linux_frame(0x100u, 8u, 0x10, 0x27);
    CanFrame out;
    ASSERT_TRUE(can_frame_from_linux(&in, &out));
    ASSERT_EQ(0x100, out.id);
    ASSERT_EQ(8, out.dlc);
    ASSERT_EQ(0x10, out.data[0]);
    ASSERT_EQ(0x27, out.data[1]);

    in = linux_frame(0x200u, 1u, 0x01, 0xFF);            /* DLC 1: data[1]은 안 가져온다 */
    ASSERT_TRUE(can_frame_from_linux(&in, &out));
    ASSERT_EQ(1, out.dlc);
    ASSERT_EQ(0x00, out.data[1]);
}

static void test_from_linux_ignored(void) {
    CanFrame out;
    struct can_frame eff = linux_frame(0x12345678u | CAN_EFF_FLAG, 8u, 0, 0);   /* 29비트 ID */
    struct can_frame rtr = linux_frame(0x100u | CAN_RTR_FLAG, 0u, 0, 0);        /* Remote 프레임 */
    struct can_frame err = linux_frame(CAN_ERR_FLAG | 0x4u, 8u, 0, 0);          /* 에러 프레임 */
    struct can_frame big = linux_frame(0x100u, 9u, 0, 0);                       /* 길이 9 */
    ASSERT_FALSE(can_frame_from_linux(&eff, &out));
    ASSERT_FALSE(can_frame_from_linux(&rtr, &out));
    ASSERT_FALSE(can_frame_from_linux(&err, &out));
    ASSERT_FALSE(can_frame_from_linux(&big, &out));
    ASSERT_FALSE(can_frame_from_linux(NULL, &out));
}

static void test_read_from_vcan0(void) {
    /* 소켓 두 개: tx로 보내고 rx로 받는다 (cansend → candump와 같다) */
    if (!vcan0_exists()) {
        printf("    (vcan0 없음: 건너뜀. ./scripts/setup_vcan.sh)\n");
        return;
    }
    int rx = can_socket_open("vcan0");
    int tx = can_socket_open("vcan0");
    ASSERT_TRUE(rx >= 0 && tx >= 0);
    if (rx < 0 || tx < 0) {
        return;
    }

    struct can_frame sent = linux_frame(0x100u, 8u, 0x10, 0x27);
    ASSERT_EQ((long)sizeof(sent), (long)write(tx, &sent, sizeof(sent)));
    CanFrame got;
    ASSERT_EQ(CAN_READ_OK, can_socket_read(rx, &got));
    ASSERT_EQ(0x100, got.id);
    ASSERT_EQ(0x27, got.data[1]);

    struct can_frame eff = linux_frame(0x1ABCDEFu | CAN_EFF_FLAG, 2u, 0, 0);
    ASSERT_EQ((long)sizeof(eff), (long)write(tx, &eff, sizeof(eff)));
    ASSERT_EQ(CAN_READ_SKIP, can_socket_read(rx, &got));           /* 받긴 받지만 무시 */

    can_socket_close(rx);
    can_socket_close(tx);
}

int main(void) {
    printf("[test_can_socket]\n");
    RUN_TEST(test_open_vcan0);
    RUN_TEST(test_open_unknown_iface);
    RUN_TEST(test_open_bad_name);
    RUN_TEST(test_close_negative_is_safe);
    RUN_TEST(test_from_linux_standard);
    RUN_TEST(test_from_linux_ignored);
    RUN_TEST(test_read_from_vcan0);
    return TEST_SUMMARY();
}
