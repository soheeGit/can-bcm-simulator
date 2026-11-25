/* SocketCAN 소켓 (M5, Linux 전용)
 *
 * Linux 커널은 CAN 장치를 eth0 같은 네트워크 장치로 다룬다(SocketCAN).
 * 그래서 CAN도 TCP처럼 socket() → bind()로 연결하고, read()/write()로 프레임을 주고받는다.
 *
 *   TCP:  socket(AF_INET, SOCK_STREAM, 0) → connect(IP:포트)  → read/write
 *   CAN:  socket(PF_CAN,  SOCK_RAW, CAN_RAW) → bind(vcan0)   → read/write (struct can_frame)
 *
 * macOS에는 SocketCAN이 없어서 이 모듈은 Linux에서만 빌드된다 (Makefile에서 uname -s로 구분).
 */
#ifndef CAN_SOCKET_H
#define CAN_SOCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <linux/can.h>

#include "can_frame.h"

/* ifname(예: "vcan0")에 묶인 CAN_RAW 소켓을 연다.
 * 반환값: 파일 디스크립터(0 이상) / 실패하면 -1 (원인은 perror로 stderr에 찍고, errno에 남는다)
 *   - 이름이 NULL, 비었거나 너무 김(15자 초과) → -1, errno = EINVAL
 *   - 그런 장치가 없음(예: "nope0")              → -1, errno = ENODEV
 * 다 쓰면 can_socket_close()로 닫는다 */
int can_socket_open(const char *ifname);

/* 소켓을 닫는다. fd가 음수면 아무것도 안 한다 */
void can_socket_close(int fd);

/* ---------- 받기 (M5-4) ---------- */

/* Linux의 struct can_frame → 우리 CanFrame.
 * 우리가 다루는 건 일반(11비트) 데이터 프레임뿐이라, 아래는 false(무시):
 *   - Extended(29비트) ID 프레임 (CAN_EFF_FLAG)
 *   - Remote 프레임 (CAN_RTR_FLAG)
 *   - 에러 프레임 (CAN_ERR_FLAG)
 *   - 길이가 8을 넘는 것, NULL */
bool can_frame_from_linux(const struct can_frame *in, CanFrame *out);

/* can_socket_read()의 결과 */
typedef enum {
    CAN_READ_OK = 0,     /* 우리 프레임 하나를 받음 → out에 채움 */
    CAN_READ_SKIP,       /* 받았지만 우리가 안 다루는 종류 (EFF/RTR/ERR) → 무시하고 다음으로 */
    CAN_READ_ERROR       /* read() 실패 또는 크기가 이상함 (원인은 perror, errno) */
} CanReadResult;

/* 프레임 하나를 받는다. 올 때까지 기다린다(blocking).
 * read(fd, &frame, sizeof(struct can_frame))가 정확히 그 크기를 돌려줘야 정상 */
CanReadResult can_socket_read(int fd, CanFrame *out);

/* ---------- 보내기 (M5-6) ---------- */

/* 우리 CanFrame → Linux의 struct can_frame (can_frame_from_linux의 반대 방향).
 * Standard ID(0x7FF 이하), DLC 8 이하만 true. 아니면(또는 NULL) false */
bool can_frame_to_linux(const CanFrame *in, struct can_frame *out);

/* 프레임 하나를 보낸다. 0 = 성공, -1 = 실패 (변환 실패면 errno = EINVAL, write 실패면 perror)
 * write(fd, &frame, sizeof(struct can_frame))가 정확히 그 크기를 돌려줘야 성공.
 * 실제 CAN에서는 받는 노드가 없어 ACK가 안 오면 송신 큐가 차서 ENOBUFS로 실패할 수 있다 */
int can_socket_write(int fd, const CanFrame *f);

/* ---------- 수신 필터 (M5-7) ---------- */

/* 필터는 최대 몇 개까지 (우리는 3개면 충분. 커널은 더 많이 받는다) */
#define CAN_SOCKET_MAX_FILTERS 16u

/* "정확히 이 ID의 일반(11비트) 데이터 프레임만"을 뜻하는 필터 한 칸을 만든다.
 * 커널 규칙: (받은 can_id & can_mask) == (filter.can_id & can_mask) 이면 통과 (candump의 ID:마스크와 같다) */
void can_filter_exact(struct can_filter *f, uint32_t id);

/* fd가 ids에 있는 ID만 받게 한다 (커널이 걸러 준다. 나머지는 read()에 아예 안 온다)
 * - count가 0이면 아무것도 안 받는다 (커널 규칙)
 * - ids가 NULL인데 count > 0, 또는 count > CAN_SOCKET_MAX_FILTERS → -1, errno = EINVAL
 * 반환값: 0 = 성공, -1 = 실패 (perror) */
int can_socket_set_filter(int fd, const uint32_t *ids, size_t count);

#endif
