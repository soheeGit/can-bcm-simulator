/* 프레임 처리 루프 (M5-5): 어떤 CanSource에서든 프레임을 받아 BCM까지 흘려보내는 "하나의" 루프
 *
 *   CanSource ──read_frame()──→ CanFrame + 시각 ──can_dispatch()──→ VehicleInputs
 *   (파일 / 소켓 / 테스트용 가짜)                       ──bcm_step()──→ BCM 상태 (M4)
 *                                                       ──bcm_tx_poll()──→ 0x400 ──CanSink──→ 파일 / 소켓 (M5-6)
 *
 * 값이 바뀌면 INFO, 모든 프레임은 DEBUG 로그로 찍는다 (logger.h, M3-5).
 * BCM 상태가 바뀌면 "STATE IDLE -> DRIVING_LOCKED (T3: ...)"를 INFO로 찍는다 (M4-3).
 * 이 루프는 소스가 무엇인지 모른다. 파일 모드와 실시간 모드가 같은 코드로 판단한다.
 * 프레임을 최대 BCM_TICK_MS(10ms)만 기다리고, 안 오면 그때도 판단한다 (M5-8, 주기 판단).
 */
#ifndef FRAME_LOOP_H
#define FRAME_LOOP_H

#include <stddef.h>
#include <stdint.h>

#include "bcm.h"
#include "can_sink.h"
#include "can_source.h"
#include "dispatcher.h"

/* 상태 전이를 앞에서부터 몇 개까지 기록할지 (시나리오 통합 테스트용, M4-7). 넘치면 횟수만 센다 */
#define FRAME_LOOP_MAX_TRANSITIONS 16u

/* 상태 전이 한 번의 기록: "몇 초에, 어디서 어디로, 어떤 규칙으로" */
typedef struct {
    uint32_t t_ms;
    BcmState from;
    BcmState to;
    BcmRule  rule;
} BcmTransitionRecord;

typedef struct {
    /* 파일 소스만 채운다 (log_read_stream이 FileSource에서 옮겨 적는다). 다른 소스는 0 */
    uint32_t lines;          /* 읽은 줄 수 (빈 줄, 주석 포함) */
    uint32_t skipped;        /* 빈 줄 / 주석 */
    uint32_t parse_errors;   /* 형식이 틀린 줄 (너무 긴 줄 포함) */
    /* 루프가 채운다 */
    uint32_t frames;         /* 소스에서 받은 프레임 수 */
    uint32_t ticks;          /* 프레임 없이 시간만 흘러서(TIMEOUT) 판단한 횟수 (M5-8). 파일 모드는 0 */
    DispatchStats dispatch;  /* 받은 프레임의 디스패처 결과 (ok / ignored / error) */
    uint32_t bcm_transitions;  /* BCM 상태가 바뀐 횟수 (bcm이 NULL이면 0) */
    uint32_t bcm_lock_cmds;    /* 도어 잠금 명령 횟수 */
    uint32_t tx_frames;        /* 만든 0x400 프레임 수 (M4-8) */
    uint32_t tx_errors;        /* 출력(sink)에 내보내다 실패한 횟수 (M5-6). 실패해도 BCM은 계속 돈다 */
    BcmTransitionRecord transitions[FRAME_LOOP_MAX_TRANSITIONS];   /* 앞에서부터 최대 16개 */
} FrameLoopStats;

/* src가 END나 ERROR를 줄 때까지 프레임마다: 디스패처 → BCM 판단 → 0x400 송신
 * - inputs: 프레임을 반영할 차량 상태 (미리 vehicle_inputs_init 해 둘 것)
 * - bcm: 프레임마다 bcm_step을 돌릴 BCM (미리 bcm_init 해 둘 것). NULL이면 BCM 없이 디스패처까지만
 * - sinks, sink_count: BCM이 만든 0x400을 내보낼 곳들 (M5-6). 0개면(NULL, 0) 만들기만 하고 안 내보낸다
 * - stats: 결과 통계 (0으로 초기화해서 채운다)
 * 반환값: 0 = 소스가 끝남(END), -1 = 인자가 NULL이거나 소스가 ERROR */
int frame_loop_run(const CanSource *src, VehicleInputs *inputs, BcmContext *bcm,
                   const CanSink *sinks, size_t sink_count, FrameLoopStats *stats);

#endif
