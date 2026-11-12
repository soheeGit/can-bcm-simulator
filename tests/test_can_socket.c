/* SocketCAN 소켓 테스트: 열기(M5-3), 받기(M5-4), 보내기(M5-6), 수신 필터(M5-7). Linux 전용
 *
 * vcan0이 있어야 하는 테스트는, vcan0이 없으면 건너뛴다 (VM에서 ./scripts/setup_vcan.sh 먼저)
 * 다른 테스트들은 /dev/null로 stderr를 버리지 않으니, 실패 케이스의 perror 메시지가 화면에 같이 보인다 (정상)
 */
#include <errno.h>
#include <net/if.h>
#include <poll.h>
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

/* ---------- 보내기 (M5-6) ---------- */

static CanFrame our_frame(uint32_t id, uint8_t dlc) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = id;
    f.dlc = dlc;
    for (uint8_t i = 0; i < CAN_FRAME_MAX_DLC; i++) {
        f.data[i] = (uint8_t)(0xA0u + i);
    }
    return f;
}

static void test_to_linux_standard(void) {
    CanFrame in = our_frame(0x400u, 8u);
    struct can_frame out;
    memset(&out, 0xFF, sizeof(out));                       /* 쓰레기로 채워 두고, 다 덮이는지 */
    ASSERT_TRUE(can_frame_to_linux(&in, &out));
    ASSERT_EQ(0x400u, out.can_id);                         /* 표시(flag) 비트 없음 */
    ASSERT_EQ(8, out.len);
    ASSERT_EQ(0xA0, out.data[0]);
    ASSERT_EQ(0xA7, out.data[7]);

    in = our_frame(0x7FFu, 2u);                            /* DLC 2: 뒤쪽 6바이트는 0 */
    ASSERT_TRUE(can_frame_to_linux(&in, &out));
    ASSERT_EQ(2, out.len);
    ASSERT_EQ(0xA1, out.data[1]);
    ASSERT_EQ(0x00, out.data[2]);
    ASSERT_EQ(0x00, out.__pad);                            /* 예약 칸도 0 */

    /* 왕복: 우리 → 리눅스 → 우리 = 처음과 같다 */
    CanFrame back;
    in = our_frame(0x123u, 5u);
    ASSERT_TRUE(can_frame_to_linux(&in, &out));
    ASSERT_TRUE(can_frame_from_linux(&out, &back));
    ASSERT_EQ(in.id, back.id);
    ASSERT_EQ(in.dlc, back.dlc);
    ASSERT_TRUE(memcmp(in.data, back.data, in.dlc) == 0);
}

static void test_to_linux_rejects(void) {
    struct can_frame out;
    CanFrame big_id = our_frame(0x800u, 8u);               /* 11비트 밖 */
    CanFrame big_dlc = our_frame(0x100u, 9u);
    ASSERT_FALSE(can_frame_to_linux(&big_id, &out));
    ASSERT_FALSE(can_frame_to_linux(&big_dlc, &out));
    ASSERT_FALSE(can_frame_to_linux(NULL, &out));
    ASSERT_FALSE(can_frame_to_linux(&big_id, NULL));
}

static void test_write_to_vcan0(void) {
    /* 우리 can_socket_write로 보내고, 다른 소켓에서 받는다 (cansend → candump) */
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

    CanFrame sent = our_frame(0x400u, 8u);
    int w = can_socket_write(tx, &sent);
    ASSERT_EQ(0, w);
    if (w == 0) {                                          /* 못 보냈으면 read()가 영원히 기다리니 건너뛴다 */
        struct can_frame raw;
        ASSERT_EQ((long)sizeof(raw), (long)read(rx, &raw, sizeof(raw)));
        ASSERT_EQ(0x400u, raw.can_id);
        ASSERT_EQ(8, raw.len);
        ASSERT_EQ(0xA7, raw.data[7]);
    }

    printf("    (아래 프레임 에러 메시지는 일부러 낸 것)\n");
    CanFrame bad = our_frame(0x800u, 8u);
    errno = 0;
    ASSERT_EQ(-1, can_socket_write(tx, &bad));             /* 변환 실패 → 보내지 않음 */
    ASSERT_EQ(EINVAL, errno);

    can_socket_close(rx);
    can_socket_close(tx);
}

/* ---------- 수신 필터 (M5-7) ---------- */

/* 최대 100ms만 기다려서 하나 받아 보기. 없으면 -1
 * (필터에 막혀 "안 왔다"를 확인하려면 끝없이 기다리는 read()는 못 쓴다. poll은 M5-8에서 자세히) */
static int try_read_id(int fd, uint32_t *id) {
    struct pollfd p = { .fd = fd, .events = POLLIN };
    if (poll(&p, 1, 100) <= 0) {
        return -1;
    }
    struct can_frame raw;
    ssize_t n = read(fd, &raw, sizeof(raw));
    if (n != (ssize_t)sizeof(raw)) {
        return -1;
    }
    *id = raw.can_id;
    return 0;
}

static void send_id(int tx, canid_t id) {
    struct can_frame f = linux_frame(id, 1u, 0x00, 0x00);
    ASSERT_EQ((long)sizeof(f), (long)write(tx, &f, sizeof(f)));
}

static void test_filter_exact_mask(void) {
    struct can_filter f;
    can_filter_exact(&f, 0x200u);
    ASSERT_EQ(0x200u, f.can_id);
    ASSERT_EQ(CAN_SFF_MASK | CAN_EFF_FLAG | CAN_RTR_FLAG, f.can_mask);
    /* 커널 규칙으로 직접 계산: (받은 ID & mask) == (0x200 & mask) 인가 */
    ASSERT_TRUE((0x200u & f.can_mask) == (f.can_id & f.can_mask));
    ASSERT_FALSE((0x201u & f.can_mask) == (f.can_id & f.can_mask));
    ASSERT_FALSE((0x600u & f.can_mask) == (f.can_id & f.can_mask));                  /* 0x200:3FF였다면 통과했을 것 */
    ASSERT_FALSE(((0x200u | CAN_RTR_FLAG) & f.can_mask) == (f.can_id & f.can_mask));  /* Remote 0x200 */
    ASSERT_FALSE(((0x200u | CAN_EFF_FLAG) & f.can_mask) == (f.can_id & f.can_mask));  /* 29비트 ID 0x200 */
}

static void test_filter_bad_args(void) {
    printf("    (아래 필터 목록 에러 메시지 2개는 일부러 낸 것)\n");
    uint32_t ids[CAN_SOCKET_MAX_FILTERS + 1u] = {0};
    errno = 0;
    ASSERT_EQ(-1, can_socket_set_filter(0, NULL, 1u));
    ASSERT_EQ(EINVAL, errno);
    errno = 0;
    ASSERT_EQ(-1, can_socket_set_filter(0, ids, CAN_SOCKET_MAX_FILTERS + 1u));
    ASSERT_EQ(EINVAL, errno);
}

static void test_filter_on_vcan0(void) {
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
    const uint32_t ids[] = { 0x100u, 0x300u };
    ASSERT_EQ(0, can_socket_set_filter(rx, ids, 2u));

    send_id(tx, 0x200u);                                   /* 막힘 */
    send_id(tx, 0x100u);                                   /* 통과 */
    send_id(tx, 0x7FFu);                                   /* 막힘 */
    send_id(tx, 0x100u | CAN_EFF_FLAG);                    /* 29비트 0x100 → 막힘 */
    send_id(tx, 0x300u | CAN_RTR_FLAG);                    /* Remote 0x300 → 막힘 */
    send_id(tx, 0x300u);                                   /* 통과 */

    uint32_t id = 0u;
    ASSERT_EQ(0, try_read_id(rx, &id));
    ASSERT_EQ(0x100u, id);
    ASSERT_EQ(0, try_read_id(rx, &id));
    ASSERT_EQ(0x300u, id);
    ASSERT_EQ(-1, try_read_id(rx, &id));                   /* 나머지 4개는 커널이 걸렀다 */

    /* 0개 = 아무것도 안 받음 */
    ASSERT_EQ(0, can_socket_set_filter(rx, NULL, 0u));
    send_id(tx, 0x100u);
    ASSERT_EQ(-1, try_read_id(rx, &id));

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
    RUN_TEST(test_to_linux_standard);
    RUN_TEST(test_to_linux_rejects);
    RUN_TEST(test_write_to_vcan0);
    RUN_TEST(test_filter_exact_mask);
    RUN_TEST(test_filter_bad_args);
    RUN_TEST(test_filter_on_vcan0);
    return TEST_SUMMARY();
}
