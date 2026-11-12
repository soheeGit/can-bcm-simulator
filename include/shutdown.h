/* 안전한 종료 (M5-9): Ctrl+C(SIGINT), kill(SIGTERM)을 받으면 바로 죽지 않고 "멈춰" 표시만 세운다
 *
 *   Ctrl+C ──커널──→ on_signal() : 표시만 세움 (여기서는 거의 아무것도 하면 안 된다)
 *                         ↓
 *   socket_source_read : 표시를 보면 CAN_SOURCE_END ("스트림 끝")
 *                         ↓
 *   frame_loop_run → 0 반환 → main: 소켓 닫기, 파일 닫기, 요약 출력, 종료 코드 0
 *
 * Java로: Runtime.getRuntime().addShutdownHook(...) + volatile boolean running 플래그
 * 한 번 받으면 핸들러가 기본값으로 돌아간다 → 정리 중에 또 Ctrl+C를 누르면 강제 종료 (멈췄을 때 탈출구)
 */
#ifndef SHUTDOWN_H
#define SHUTDOWN_H

#include <stdbool.h>

/* SIGINT, SIGTERM 핸들러를 건다. 0 = 성공, -1 = 실패 (perror) */
int shutdown_install(void);

/* 종료 요청이 들어왔나 */
bool shutdown_requested(void);

/* 어떤 시그널로 요청됐나 (SIGINT = 2, SIGTERM = 15). 요청이 없으면 0 */
int shutdown_signal(void);

/* 테스트용: 시그널 없이 직접 요청 / 요청 지우기 */
void shutdown_request(int sig);
void shutdown_reset(void);

#endif
