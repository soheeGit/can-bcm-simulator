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

/* ifname(예: "vcan0")에 묶인 CAN_RAW 소켓을 연다.
 * 반환값: 파일 디스크립터(0 이상) / 실패하면 -1 (원인은 perror로 stderr에 찍고, errno에 남는다)
 *   - 이름이 NULL, 비었거나 너무 김(15자 초과) → -1, errno = EINVAL
 *   - 그런 장치가 없음(예: "nope0")              → -1, errno = ENODEV
 * 다 쓰면 can_socket_close()로 닫는다 */
int can_socket_open(const char *ifname);

/* 소켓을 닫는다. fd가 음수면 아무것도 안 한다 */
void can_socket_close(int fd);

#endif
