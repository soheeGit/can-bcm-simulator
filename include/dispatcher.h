/* 메시지 디스패처 — 들어온 프레임의 ID를 보고 알맞은 디코더를 불러 "차량 입력 상태"를 갱신한다
 *
 *   CanFrame ──can_dispatch()──┬─ 0x100 → decode_vehicle_speed → in->speed
 *                              ├─ 0x200 → decode_door_status   → in->doors
 *                              ├─ 0x300 → decode_ign_status    → in->ign
 *                              └─ 그 외 → 무시 (CAN은 브로드캐스트라 관심 없는 메시지가 계속 온다)
 *
 * 웹의 URL 라우터(경로 → Controller)와 같은 역할. M4 상태 머신은 VehicleInputs만 보고 판단한다.
 */
#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"
#include "signals.h"

/* BCM이 판단에 쓰는 "지금 차량 상태"의 모음 */
typedef struct {
    VehicleSpeed speed;
    DoorStatus   doors;
    IgnStatus    ign;

    /* 한 번이라도 받았는가 (시동 직후에는 아직 아무것도 안 왔을 수 있다) */
    bool speed_received;
    bool doors_received;
    bool ign_received;

    /* 마지막으로 정상 수신한 시각 (ms). M4-6 타임아웃 판단에 쓴다 */
    uint32_t speed_rx_ms;
    uint32_t doors_rx_ms;
    uint32_t ign_rx_ms;
} VehicleInputs;

/* 처리 결과 */
typedef enum {
    DISPATCH_OK = 0,      /* 해석해서 상태를 갱신함 */
    DISPATCH_IGNORED,     /* 관심 없는 ID → 그냥 넘어감 (정상 상황) */
    DISPATCH_ERROR        /* 우리 메시지인데 해석 불가 (짧은 DLC 등) → 상태는 그대로 */
} DispatchResult;

/* 처리/무시/오류 건수 (로그, 디버깅용) */
typedef struct {
    uint32_t ok;
    uint32_t ignored;
    uint32_t error;
} DispatchStats;

/* 아무것도 안 받은 상태로 초기화 */
void vehicle_inputs_init(VehicleInputs *in);

/* 프레임 하나를 처리한다.
 * - 해석에 실패하면 in의 이전 값을 그대로 둔다 (깨진 프레임 하나로 상태가 망가지지 않게)
 * - stats는 NULL이어도 된다 (통계가 필요 없을 때) */
DispatchResult can_dispatch(const CanFrame *f, VehicleInputs *in, uint32_t now_ms,
                            DispatchStats *stats);

/* 결과 이름 (로그용) */
const char *dispatch_result_name(DispatchResult r);

#endif
