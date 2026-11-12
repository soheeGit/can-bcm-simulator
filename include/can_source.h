/* CAN 입력 소스 — "프레임을 하나씩 주는 것"의 공통 인터페이스 (M5-5)
 *
 * BCM 입장에서는 프레임이 파일(M3)에서 오든 소켓(M5)에서 오든 상관없다.
 * 그래서 "프레임 하나 주세요"만 약속(인터페이스)으로 정하고, 구현은 갈아 끼운다.
 *
 *   Java                                     C
 *   interface CanSource {                    typedef struct {
 *       Result readFrame(...);                   void *self;                       ← 구현 객체 (this)
 *   }                                            CanSourceResult (*read_frame)(void *self, ...);  ← 메서드
 *   class FileSource implements CanSource    } CanSource;
 *   class SocketSource implements CanSource  file_source.h / socket_source.h
 *
 * C에는 interface도 this도 없어서, "함수 포인터 + self 포인터"를 직접 묶어서 들고 다닌다.
 *   src.read_frame(src.self, &f, &t)   ==   src.readFrame(&f, &t)   (Java가 this를 몰래 넘기는 걸 손으로 한다)
 */
#ifndef CAN_SOURCE_H
#define CAN_SOURCE_H

#include <stdint.h>

#include "can_frame.h"

typedef enum {
    CAN_SOURCE_OK = 0,     /* 프레임 하나를 out에, 시각(ms)을 ts_ms에 채움 */
    CAN_SOURCE_TIMEOUT,    /* timeout_ms 동안 프레임이 안 옴 (M5-8). ts_ms에는 "지금" 시각 */
    CAN_SOURCE_END,        /* 더 없음 (파일 끝). 정상 종료 */
    CAN_SOURCE_ERROR       /* 읽기 실패 (원인은 각 소스가 로그로 찍는다) */
} CanSourceResult;

/* read_frame의 timeout_ms에 넘기면 "올 때까지 끝없이 기다림" */
#define CAN_SOURCE_WAIT_FOREVER (-1)

typedef struct {
    void *self;            /* 구현 객체 (FileSource*, SocketSource* ...). 인터페이스는 안을 모른다 */
    const char *name;      /* 로그용 이름 ("file", "socket") */
    /* 다음 프레임 하나. 우리가 안 다루는 줄/프레임(주석, 형식 오류, 29비트 ID ...)은 소스가 알아서 건너뛰고
     * "쓸 수 있는 프레임"만 돌려준다. ts_ms는 소스가 시작된 뒤로 흐른 시간
     * timeout_ms: 최대 이만큼만 기다린다 (M5-8). 지나면 CAN_SOURCE_TIMEOUT.
     *   CAN_SOURCE_WAIT_FOREVER면 끝없이. 파일처럼 "기다림"이 없는 소스는 무시해도 된다 (TIMEOUT을 안 줌) */
    CanSourceResult (*read_frame)(void *self, CanFrame *out, uint32_t *ts_ms, int timeout_ms);
} CanSource;

#endif
