/* 시나리오 로그 생성기 (M3-4)
 *
 *   make scenarios   → data/scenarios 폴더의 .log 파일 4개를 다시 만든다
 *
 * 100ms마다(정의서의 송신 주기) 0x300 → 0x200 → 0x100 프레임을 하나씩 찍는다.
 * 신호 값은 can_set_bits()(M2-2)로 정의서 위치에 넣는다 → 손으로 16진수를 계산하지 않아도 된다.
 * 출력은 candump -l 형식 (docs/log-format.md). 시나리오 파일에는 주석을 넣지 않는다 (M3-1).
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "can_signal.h"
#include "signals.h"

#define TICK_MS      100u     /* 정의서 송신 주기 */
#define IFACE        "vcan0"

/* 한 시점(100ms)의 차량 상태. 기어는 무효값(7)도 넣을 수 있게 raw 숫자로 둔다 */
typedef struct {
    uint8_t  ign;            /* IgnState raw 0~3 */
    uint8_t  gear;           /* Gear raw 0~7 */
    uint16_t speed;          /* VehicleSpeed raw (0.01 km/h), 0xFFFF = 무효 */
    uint8_t  doors;          /* bit0 FL ... bit4 TRUNK */
    bool     send_speed;     /* false면 이 시점에 0x100을 안 보냄 (신호 끊김) */
} Tick;

/* ---------- 출력 ---------- */

static void emit_raw_line(FILE *out, uint32_t t_ms, const char *rest) {
    fprintf(out, "(%u.%06u) %s %s\n", t_ms / 1000u, (t_ms % 1000u) * 1000u, IFACE, rest);
}

static void emit_frame(FILE *out, uint32_t t_ms, uint32_t id, const uint8_t data[8], uint8_t dlc) {
    fprintf(out, "(%u.%06u) %s %03X#", t_ms / 1000u, (t_ms % 1000u) * 1000u, IFACE, (unsigned)id);
    for (uint8_t i = 0; i < dlc; i++) {
        fprintf(out, "%02X", data[i]);
    }
    fprintf(out, "\n");
}

/* 한 시점의 상태를 프레임 3개로 만든다 (신호 위치는 signals.h 상수 = 정의서) */
static void emit_tick(FILE *out, uint32_t t_ms, const Tick *s) {
    uint8_t data[8];

    for (int i = 0; i < 8; i++) { data[i] = 0; }
    can_set_bits(data, IGN_STATE_START_BIT, IGN_STATE_LENGTH, s->ign);
    can_set_bits(data, GEAR_START_BIT, GEAR_LENGTH, s->gear);
    emit_frame(out, t_ms, MSG_ID_IGN_STATUS, data, 8);

    for (int i = 0; i < 8; i++) { data[i] = 0; }
    can_set_bits(data, 0, 5, s->doors);                       /* FL~TRUNK 5비트 한 번에 */
    emit_frame(out, t_ms, MSG_ID_DOOR_STATUS, data, 8);

    if (s->send_speed) {
        for (int i = 0; i < 8; i++) { data[i] = 0; }
        can_set_bits(data, VEHICLE_SPEED_START_BIT, VEHICLE_SPEED_LENGTH, s->speed);
        emit_frame(out, t_ms, MSG_ID_VEHICLE_SPEED, data, 8);
    }
}

/* ---------- 공통: 속도 변화 도우미 ---------- */

/* from에서 to까지 steps번에 나눠 바뀔 때, i번째(0 ~ steps) 값 */
static uint16_t ramp(uint16_t from, uint16_t to, uint32_t i, uint32_t steps) {
    if (i >= steps) {
        return to;
    }
    int32_t diff = (int32_t)to - (int32_t)from;
    return (uint16_t)((int32_t)from + diff * (int32_t)i / (int32_t)steps);
}

#define KMH(x) ((uint16_t)((x) * 100u))   /* km/h → raw (0.01 km/h) */

/* 아래 시나리오 함수의 i는 "몇 번째 100ms인가" (i = 35 → 3.5초) */

/* ---------- 01 정상 주행 ---------- */
/* 시동 ON → D → 0→60 km/h 가속 → 정속 → 감속 → 정차 → P → 시동 OFF */
static void scenario_normal_drive(uint32_t i, Tick *s) {
    s->ign = (i < 5u || i >= 100u) ? 0u : 2u;                  /* 0.5초에 ON, 10.0초에 OFF */
    s->gear = (i >= 10u && i < 95u) ? 3u : 0u;                 /* 1.0초 D, 9.5초 P */
    s->doors = 0u;
    s->send_speed = true;
    if (i < 10u)       { s->speed = 0u; }
    else if (i < 40u)  { s->speed = ramp(0u, KMH(60u), i - 10u, 30u); }    /* 1.0~4.0초 가속 */
    else if (i < 60u)  { s->speed = KMH(60u); }                           /* 4.0~6.0초 정속 */
    else if (i < 90u)  { s->speed = ramp(KMH(60u), 0u, i - 60u, 30u); }    /* 6.0~9.0초 감속 */
    else               { s->speed = 0u; }
}

/* ---------- 02 주행 중 운전석 문 열림 ---------- */
/* 40 km/h로 달리다 4.0~5.0초에 운전석 문이 열렸다 닫힘 */
static void scenario_door_open(uint32_t i, Tick *s) {
    s->ign = (i < 5u || i >= 100u) ? 0u : 2u;
    s->gear = (i >= 10u && i < 95u) ? 3u : 0u;
    s->send_speed = true;
    if (i < 10u)       { s->speed = 0u; }
    else if (i < 30u)  { s->speed = ramp(0u, KMH(40u), i - 10u, 20u); }    /* 1.0~3.0초 가속 */
    else if (i < 70u)  { s->speed = KMH(40u); }                           /* 3.0~7.0초 정속 */
    else if (i < 90u)  { s->speed = ramp(KMH(40u), 0u, i - 70u, 20u); }    /* 7.0~9.0초 감속 */
    else               { s->speed = 0u; }
    s->doors = (i >= 40u && i < 50u) ? 0x01u : 0u;             /* 4.0~5.0초 FL 열림 */
}

/* ---------- 03 차속 신호 끊김 ---------- */
/* 50 km/h로 달리는 중 3.0~4.0초 동안 0x100(차속)만 안 옴. 나머지 메시지는 계속 옴 */
static void scenario_signal_timeout(uint32_t i, Tick *s) {
    s->ign = (i < 5u || i >= 80u) ? 0u : 2u;
    s->gear = (i >= 10u && i < 75u) ? 3u : 0u;
    s->doors = 0u;
    if (i < 10u)       { s->speed = 0u; }
    else if (i < 20u)  { s->speed = ramp(0u, KMH(50u), i - 10u, 10u); }    /* 1.0~2.0초 가속 */
    else if (i < 50u)  { s->speed = KMH(50u); }
    else if (i < 70u)  { s->speed = ramp(KMH(50u), 0u, i - 50u, 20u); }
    else               { s->speed = 0u; }
    s->send_speed = !(i >= 30u && i < 40u);                     /* 3.0~4.0초 끊김 (10번 = 1초) */
}

/* ---------- 04 이상한 프레임 섞임 ---------- */
/* 30 km/h 주행 중간중간 이상한 줄과 이상한 값을 끼워 넣음 (파일 모드 전용, M5 재생에는 안 씀) */
static void scenario_invalid_frames(uint32_t i, Tick *s) {
    s->ign = (i < 2u || i >= 40u) ? 0u : 2u;
    s->gear = (i >= 3u && i < 38u) ? 3u : 0u;
    s->doors = 0u;
    s->send_speed = true;
    if (i < 3u)        { s->speed = 0u; }
    else if (i < 13u)  { s->speed = ramp(0u, KMH(30u), i - 3u, 10u); }
    else if (i < 28u)  { s->speed = KMH(30u); }
    else if (i < 38u)  { s->speed = ramp(KMH(30u), 0u, i - 28u, 10u); }
    else               { s->speed = 0u; }

    if (i == 18u) { s->speed = 0xFFFFu; }                       /* 1.8초: 차속 무효값 */
    if (i == 22u) { s->gear = 7u; }                             /* 2.2초: 기어 무효값 */
}

static void inject_invalid_lines(FILE *out, uint32_t i, uint32_t t_ms) {
    switch (i) {
        case 15u: emit_raw_line(out, t_ms + 10u, "555#AABBCCDD"); break;               /* 관심 없는 ID */
        case 16u: emit_raw_line(out, t_ms + 10u, "100#10"); break;                     /* 차속인데 DLC 1 */
        case 17u: emit_raw_line(out, t_ms + 10u, "100#10G7000000000000"); break;       /* 16진수 아님 */
        case 19u: emit_raw_line(out, t_ms + 10u, "100#102"); break;                    /* 홀수 자리 */
        case 20u: emit_raw_line(out, t_ms + 10u, "800#00"); break;                     /* ID > 0x7FF */
        case 21u: emit_raw_line(out, t_ms + 10u, "100#R"); break;                      /* Remote Frame */
        case 23u: fprintf(out, "garbage line\n"); break;                               /* 완전히 깨진 줄 */
        case 24u: emit_raw_line(out, t_ms + 10u, "400#0101000000000005"); break;       /* 우리 BCM 메시지 */
        default: break;
    }
}

/* ---------- 파일 만들기 ---------- */

typedef void (*ScenarioFn)(uint32_t i, Tick *s);

static int write_scenario(const char *dir, const char *name, ScenarioFn fn, uint32_t ticks, bool inject) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        perror(path);
        return -1;
    }
    for (uint32_t i = 0; i <= ticks; i++) {
        Tick s = {0};
        fn(i, &s);
        uint32_t t_ms = i * TICK_MS;
        emit_tick(out, t_ms, &s);
        if (inject) {
            inject_invalid_lines(out, i, t_ms);
        }
    }
    fclose(out);
    printf("생성: %s\n", path);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *dir = (argc > 1) ? argv[1] : "data/scenarios";
    int err = 0;
    err |= write_scenario(dir, "01_normal_drive.log",            scenario_normal_drive,    105u, false);
    err |= write_scenario(dir, "02_door_open_while_driving.log", scenario_door_open,       105u, false);
    err |= write_scenario(dir, "03_signal_timeout.log",          scenario_signal_timeout,  85u,  false);
    err |= write_scenario(dir, "04_invalid_frames.log",          scenario_invalid_frames,  42u,  true);
    return err == 0 ? 0 : 1;
}
