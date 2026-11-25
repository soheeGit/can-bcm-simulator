/* 소켓 열기 실습 (M5-3, Linux 전용)
 *
 *   make socket_demo
 *   ./build/can_socket_demo vcan0     # 열고 fd 번호를 보여 준 뒤 Enter를 기다린다
 *   ./build/can_socket_demo nope0     # 없는 장치 → 에러
 *
 * 기다리는 동안 다른 터미널에서:  ls -l /proc/<pid>/fd   (pid는 화면에 나온다)
 */
#include <stdio.h>
#include <unistd.h>

#include "can_socket.h"

int main(int argc, char *argv[]) {
    const char *ifname = (argc > 1) ? argv[1] : "vcan0";
    int fd = can_socket_open(ifname);
    if (fd < 0) {
        fprintf(stderr, "열기 실패: %s\n", ifname);
        return 1;
    }
    printf("%s 소켓 열림: fd = %d, pid = %d\n", ifname, fd, (int)getpid());
    printf("다른 터미널에서: ls -l /proc/%d/fd\n", (int)getpid());
    printf("Enter를 누르면 닫고 끝냅니다...");
    fflush(stdout);
    (void)getchar();
    can_socket_close(fd);
    printf("닫음\n");
    return 0;
}
