#define _DEFAULT_SOURCE   /* struct sigaction (POSIX) */
/* 안전한 종료 테스트 (M5-9) — 진짜 시그널을 자기 자신에게 보내(raise) 본다. 맥에서도 돈다 */
#include <signal.h>
#include <stdio.h>

#include "shutdown.h"
#include "test_framework.h"

static void test_initially_not_requested(void) {
    shutdown_reset();
    ASSERT_FALSE(shutdown_requested());
    ASSERT_EQ(0, shutdown_signal());
}

static void test_manual_request(void) {
    shutdown_request(SIGTERM);
    ASSERT_TRUE(shutdown_requested());
    ASSERT_EQ(SIGTERM, shutdown_signal());
    shutdown_reset();
    ASSERT_FALSE(shutdown_requested());
}

static void test_real_signals(void) {
    /* 핸들러를 걸고 진짜 시그널을 보낸다. 핸들러가 없으면 여기서 프로세스가 죽는다 (= 테스트 실패) */
    shutdown_reset();
    ASSERT_EQ(0, shutdown_install());

    ASSERT_EQ(0, raise(SIGTERM));                  /* kill <pid>, timeout 명령이 보내는 것 */
    ASSERT_TRUE(shutdown_requested());
    ASSERT_EQ(SIGTERM, shutdown_signal());

    shutdown_reset();
    ASSERT_EQ(0, raise(SIGINT));                   /* Ctrl+C */
    ASSERT_TRUE(shutdown_requested());
    ASSERT_EQ(SIGINT, shutdown_signal());

    /* SA_RESETHAND: 한 번 받은 뒤 핸들러는 기본(= 프로세스 종료)으로 돌아갔다. 확인만 하고 다시 보내지는 않는다 */
    struct sigaction now;
    ASSERT_EQ(0, sigaction(SIGINT, NULL, &now));   /* NULL = 바꾸지 않고 지금 설정만 읽기 */
    ASSERT_TRUE(now.sa_handler == SIG_DFL);
    shutdown_reset();
}

int main(void) {
    printf("[test_shutdown]\n");
    RUN_TEST(test_initially_not_requested);
    RUN_TEST(test_manual_request);
    RUN_TEST(test_real_signals);
    return TEST_SUMMARY();
}
