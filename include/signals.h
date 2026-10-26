/* 신호 디코더 — 신호 정의서(docs/signal-spec.md)대로 CAN 프레임을 "의미 있는 값"으로 바꾼다
 *
 *   CanFrame (바이트)  ──decode_xxx()──→  VehicleSpeed 등 (내부 상태, ADR-001)
 *
 * 디코더는 비트를 직접 계산하지 않고 can_get_bits()(M2-1)를 쓴다.
 * M2-3: 차속 / M2-4: 도어 / M2-5: 시동·기어 디코더가 이 모듈에 추가된다.
 */
#ifndef SIGNALS_H
#define SIGNALS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "can_frame.h"

/* ---------- 메시지 ID (정의서 2번) ---------- */

#define MSG_ID_VEHICLE_SPEED  0x100u
#define MSG_ID_DOOR_STATUS    0x200u
#define MSG_ID_IGN_STATUS     0x300u
#define MSG_ID_BCM_STATUS     0x400u

/* ---------- 0x100 VEHICLE_SPEED ---------- */

#define VEHICLE_SPEED_MIN_DLC      2u        /* 최소 길이: data[0], data[1] */
#define VEHICLE_SPEED_START_BIT    0u
#define VEHICLE_SPEED_LENGTH       16u
#define VEHICLE_SPEED_RAW_INVALID  0xFFFFu   /* 센서 고장 등 "신호 무효" */

/* 차속. 실수(float) 대신 0.01 km/h 단위 정수로 보관한다 (고정소수점)
 *   centi_kmh = 10000 → 100.00 km/h
 *   valid = false → 송신 측이 "무효(0xFFFF)"를 보냈다. centi_kmh는 쓰면 안 된다 */
typedef struct {
    bool     valid;
    uint16_t centi_kmh;
} VehicleSpeed;

/* 0x100 프레임에서 차속을 꺼낸다.
 * 반환값: 프레임을 해석할 수 있으면 true
 *   false인 경우: f/out이 NULL, 유효하지 않은 프레임, ID가 0x100이 아님, DLC < 2
 * 해석은 했지만 신호가 무효(0xFFFF)면 true를 반환하고 out->valid = false */
bool decode_vehicle_speed(const CanFrame *f, VehicleSpeed *out);

/* "100.00 km/h" / "INVALID" 형태의 문자열로 만든다 (로그용, float 없이 정수로) */
void vehicle_speed_format(const VehicleSpeed *s, char *buf, size_t size);

/* ---------- 0x200 DOOR_STATUS ---------- */

#define DOOR_STATUS_MIN_DLC  1u    /* 최소 길이: data[0] */
#define DOOR_FL_BIT          0u    /* 운전석 */
#define DOOR_FR_BIT          1u    /* 조수석 */
#define DOOR_RL_BIT          2u    /* 뒷좌석 왼쪽 */
#define DOOR_RR_BIT          3u    /* 뒷좌석 오른쪽 */
#define DOOR_TRUNK_BIT       4u    /* 트렁크 */
#define DOOR_SIGNAL_LENGTH   1u    /* 신호마다 1비트 */

/* 도어 상태 (true = 열림). ADR-001: 비트필드가 아니라 bool 구조체 */
typedef struct {
    bool fl;
    bool fr;
    bool rl;
    bool rr;
    bool trunk;
} DoorStatus;

/* 0x200 프레임에서 도어 상태를 꺼낸다.
 * 반환값: 해석할 수 있으면 true (false: NULL, 유효하지 않은 프레임, ID가 0x200이 아님, DLC < 1) */
bool decode_door_status(const CanFrame *f, DoorStatus *out);

/* 문 4개 또는 트렁크 중 하나라도 열려 있으면 true (주행 중 경고 판단용, M4)
 * d가 NULL이면 false */
bool door_any_open(const DoorStatus *d);

/* "FL=1 FR=0 RL=0 RR=0 TRUNK=1" 형태의 문자열로 만든다 (로그용) */
void door_status_format(const DoorStatus *d, char *buf, size_t size);

#endif
