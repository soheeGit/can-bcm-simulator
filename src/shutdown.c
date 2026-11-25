/* sigaction, sigemptyset을 -std=c11에서 쓰려면 POSIX 이름을 켜야 한다 */
#define _DEFAULT_SOURCE

#include "shutdown.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

/* 핸들러와 메인 흐름이 같이 보는 변수
 * - volatile: "언제든 바깥(시그널)에서 바뀔 수 있으니, 매번 메모리에서 다시 읽어라" (컴파일러 최적화 금지)
 * - sig_atomic_t: 읽고 쓰는 게 "한 번에" 일어난다고 보장되는 정수 타입 (중간에 끊겨 반쯤 쓰인 값이 없다) */
static volatile sig_atomic_t g_stop_signal = 0;

/* 시그널 핸들러: 프로그램이 "어디를 실행하던 중이든" 끼어들어 불린다 (printf 도중일 수도 있다)
 * 그래서 여기서는 표시만 세운다. printf, malloc, LOG_* 같은 건 부르면 안 된다 (async-signal-safe가 아님) */
static void on_signal(int sig) {
    g_stop_signal = sig;
}

int shutdown_install(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;            /* 시그널이 오면 부를 함수 */
    sigemptyset(&sa.sa_mask);             /* 핸들러가 도는 동안 따로 막을 시그널은 없음 */
    /* 한 번 받으면 기본 동작으로 복귀 → 두 번째 Ctrl+C는 강제 종료
     * (리눅스에서 SA_RESETHAND는 0x80000000u = 맨 윗비트라, int인 sa_flags에 넣을 때 형 변환을 써 준다) */
    sa.sa_flags = (int)SA_RESETHAND;
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

bool shutdown_requested(void) {
    return g_stop_signal != 0;
}

int shutdown_signal(void) {
    return (int)g_stop_signal;
}

void shutdown_request(int sig) {
    g_stop_signal = sig;
}

void shutdown_reset(void) {
    g_stop_signal = 0;
}
