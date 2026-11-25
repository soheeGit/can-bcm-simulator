/* 프레임 루프 테스트 (M5-5) — 파일도 소켓도 아닌 "가짜 소스"를 끼워서 돌린다
 *
 * CanSource가 인터페이스라서, 테스트용 구현을 하나 더 만들면 된다 (Java의 Mockito/가짜 구현체와 같다).
 * 파일을 만들 필요도, vcan0도 필요 없다 → 맥에서도 돈다.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "frame_loop.h"
#include "logger.h"
#include "test_framework.h"

/* ---------- 가짜 소스: 배열에 넣어 둔 프레임을 차례로 준다 ---------- */

typedef struct {
    const CanFrame *frames;
    const uint32_t *ts_ms;
    size_t count;
    size_t next;
    size_t fail_at;          /* 이 번호에서 ERROR를 준다 (안 쓰면 count보다 크게) */
    uint32_t silence_until;  /* M5-8: 프레임이 다 떨어진 뒤 이 시각(ms)까지 "아무것도 안 옴"(TIMEOUT)을 흉내. 0이면 바로 END */
    uint32_t now;            /* 가짜 시계: 마지막으로 돌려준 시각 */
} ArraySource;

static CanSourceResult array_source_read(void *self, CanFrame *out, uint32_t *ts_ms, int timeout_ms) {
    ArraySource *as = self;
    if (as->next == as->fail_at) {
        return CAN_SOURCE_ERROR;
    }
    if (as->next < as->count) {
        *out = as->frames[as->next];
        *ts_ms = as->now = as->ts_ms[as->next];
        as->next++;
        return CAN_SOURCE_OK;
    }
    /* 프레임이 더 없다: 기다리는 시간만큼 시계를 돌리고 TIMEOUT (진짜 소켓의 poll처럼) */
    if (timeout_ms >= 0 && as->now + (uint32_t)timeout_ms <= as->silence_until) {
        as->now += (uint32_t)timeout_ms;
        *ts_ms = as->now;
        return CAN_SOURCE_TIMEOUT;
    }
    return CAN_SOURCE_END;
}

static CanSource array_source(ArraySource *as) {
    CanSource src = { .self = as, .name = "array", .read_frame = array_source_read };
    return src;
}

/* ---------- 가짜 출력: 받은 프레임을 배열에 모아 둔다 (M5-6) ---------- */

#define ARRAY_SINK_MAX 32u

typedef struct {
    CanFrame frames[ARRAY_SINK_MAX];
    uint32_t ts_ms[ARRAY_SINK_MAX];
    size_t count;
    bool fail;               /* true면 항상 실패 (버스 고장 흉내) */
} ArraySink;

static bool array_sink_write(void *self, const CanFrame *f, uint32_t ts_ms) {
    ArraySink *as = self;
    if (as->fail || as->count >= ARRAY_SINK_MAX) {
        return false;
    }
    as->frames[as->count] = *f;
    as->ts_ms[as->count] = ts_ms;
    as->count++;
    return true;
}

static CanSink array_sink(ArraySink *as) {
    CanSink sink = { .self = as, .name = "array", .write_frame = array_sink_write };
    return sink;
}

static CanFrame frame2(uint32_t id, uint8_t dlc, uint8_t b0, uint8_t b1) {
    CanFrame f;
    memset(&f, 0, sizeof(f));
    f.id = id;
    f.dlc = dlc;
    f.data[0] = b0;
    f.data[1] = b1;
    return f;
}

/* ---------- 테스트 ---------- */

static void test_reads_until_end(void) {
    const CanFrame frames[] = {
        frame2(0x300u, 2u, 0x02, 0x03),       /* IGN=ON GEAR=D */
        frame2(0x100u, 8u, 0x10, 0x27),       /* 100.00 km/h */
        frame2(0x7FFu, 1u, 0x00, 0x00),       /* 모르는 ID → ignored */
    };
    const uint32_t ts[] = { 0u, 100u, 250u };
    ArraySource as = { frames, ts, 3u, 0u, 99u, 0u, 0u };
    CanSource src = array_source(&as);
    VehicleInputs in;
    FrameLoopStats s;
    vehicle_inputs_init(&in);

    ASSERT_EQ(0, frame_loop_run(&src, &in, NULL, NULL, 0u, &s));      /* END → 0 */
    ASSERT_EQ(3, s.frames);
    ASSERT_EQ(2, s.dispatch.ok);
    ASSERT_EQ(1, s.dispatch.ignored);
    ASSERT_EQ(10000, in.speed.centi_kmh);
    ASSERT_EQ(100, in.speed_rx_ms);                               /* 소스가 준 시각이 그대로 */
    ASSERT_EQ(3, as.next);                                        /* 끝까지 다 꺼냈다 */
}

static void test_source_error_stops(void) {
    const CanFrame frames[] = { frame2(0x100u, 8u, 0x10, 0x27), frame2(0x100u, 8u, 0x20, 0x4E) };
    const uint32_t ts[] = { 0u, 100u };
    ArraySource as = { frames, ts, 2u, 0u, 1u, 0u, 0u };                  /* 두 번째에서 ERROR */
    CanSource src = array_source(&as);
    VehicleInputs in;
    FrameLoopStats s;
    vehicle_inputs_init(&in);

    ASSERT_EQ(-1, frame_loop_run(&src, &in, NULL, NULL, 0u, &s));
    ASSERT_EQ(1, s.frames);                                       /* 오류 전까지만 처리 */
    ASSERT_EQ(10000, in.speed.centi_kmh);
}

/* 1초 동안 100ms마다 시동 ON(D) + 문 닫힘 + 20 km/h */
static void make_drive(CanFrame frames[30], uint32_t ts[30]) {
    for (size_t i = 0; i < 10u; i++) {
        uint32_t t = (uint32_t)i * 100u;
        frames[i * 3u]      = frame2(0x300u, 2u, 0x02, 0x03);
        frames[i * 3u + 1u] = frame2(0x200u, 1u, 0x00, 0x00);
        frames[i * 3u + 2u] = frame2(0x100u, 8u, 0xD0, 0x07);   /* 0x07D0 = 2000 → 20.00 km/h */
        ts[i * 3u] = ts[i * 3u + 1u] = ts[i * 3u + 2u] = t;
    }
}

static void test_runs_bcm(void) {
    /* → T3로 잠금, 0x400도 만든다 */
    CanFrame frames[30];
    uint32_t ts[30];
    make_drive(frames, ts);
    ArraySource as = { frames, ts, 30u, 0u, 99u, 0u, 0u };
    CanSource src = array_source(&as);
    VehicleInputs in;
    BcmContext bcm;
    FrameLoopStats s;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);

    ASSERT_EQ(0, frame_loop_run(&src, &in, &bcm, NULL, 0u, &s));
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, bcm.state);
    ASSERT_EQ(1, s.bcm_transitions);
    ASSERT_EQ(BCM_RULE_T3, s.transitions[0].rule);
    ASSERT_EQ(1, s.bcm_lock_cmds);
    ASSERT_EQ(10, s.tx_frames);                                   /* 0, 100, ..., 900ms */
}

static void test_sinks_get_tx(void) {
    /* 0x400이 출력 두 곳에 똑같이 나간다 (소켓 + 파일처럼) */
    CanFrame frames[30];
    uint32_t ts[30];
    make_drive(frames, ts);
    ArraySource as = { frames, ts, 30u, 0u, 99u, 0u, 0u };
    CanSource src = array_source(&as);
    ArraySink a = {0}, b = {0};
    CanSink sinks[2] = { array_sink(&a), array_sink(&b) };
    VehicleInputs in;
    BcmContext bcm;
    FrameLoopStats s;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);

    ASSERT_EQ(0, frame_loop_run(&src, &in, &bcm, sinks, 2u, &s));
    ASSERT_EQ(10, a.count);
    ASSERT_EQ(10, b.count);
    ASSERT_EQ(0, s.tx_errors);
    ASSERT_EQ(0x400, a.frames[0].id);
    ASSERT_EQ(8, a.frames[0].dlc);
    ASSERT_EQ(0u, a.ts_ms[0]);
    ASSERT_EQ(900u, a.ts_ms[9]);                                  /* 100ms마다 */
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, a.frames[1].data[0]);     /* 잠근 뒤의 상태 */
    ASSERT_EQ(1, a.frames[9].data[7] - a.frames[8].data[7]);      /* Alive Counter가 1씩 */
    ASSERT_TRUE(memcmp(&a.frames[5], &b.frames[5], sizeof(CanFrame)) == 0);
}

static void test_sink_failure_keeps_running(void) {
    /* 한 출력이 계속 실패해도 BCM 판단과 다른 출력은 멈추지 않는다 */
    CanFrame frames[30];
    uint32_t ts[30];
    make_drive(frames, ts);
    ArraySource as = { frames, ts, 30u, 0u, 99u, 0u, 0u };
    CanSource src = array_source(&as);
    ArraySink bad = { .fail = true }, good = {0};
    CanSink sinks[2] = { array_sink(&bad), array_sink(&good) };
    VehicleInputs in;
    BcmContext bcm;
    FrameLoopStats s;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);

    ASSERT_EQ(0, frame_loop_run(&src, &in, &bcm, sinks, 2u, &s));
    ASSERT_EQ(10, s.tx_errors);                                   /* 실패는 센다 */
    ASSERT_EQ(10, s.tx_frames);
    ASSERT_EQ(10, good.count);                                    /* 다른 출력은 그대로 */
    ASSERT_EQ(BCM_STATE_DRIVING_LOCKED, bcm.state);               /* 판단도 그대로 */
    ASSERT_EQ(30, s.frames);
}

static void test_silence_keeps_judging(void) {
    /* M5-8: 1초 달리다가 프레임이 뚝 끊기고(선이 빠짐) 1.1초 동안 아무것도 안 온다.
     * 프레임이 없어도 10ms마다 판단 → 0x400은 계속 나가고, 500ms 뒤 신호 끊김으로 WARNING (T5) */
    CanFrame frames[30];
    uint32_t ts[30];
    make_drive(frames, ts);                                       /* 마지막 프레임 900ms */
    ArraySource as = { frames, ts, 30u, 0u, 99u, 2000u, 0u };    /* 2000ms까지 조용 */
    CanSource src = array_source(&as);
    ArraySink sink = {0};
    CanSink sinks[1] = { array_sink(&sink) };
    VehicleInputs in;
    BcmContext bcm;
    FrameLoopStats s;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);

    ASSERT_EQ(0, frame_loop_run(&src, &in, &bcm, sinks, 1u, &s));
    ASSERT_EQ(30, s.frames);
    ASSERT_EQ(110, s.ticks);                                      /* 910, 920, ..., 2000ms */
    ASSERT_EQ(21, s.tx_frames);                                   /* 0 ~ 2000ms, 100ms마다 (조용할 때도 11개) */
    ASSERT_EQ(2, s.bcm_transitions);
    ASSERT_EQ(BCM_RULE_T5, s.transitions[1].rule);                /* 신호 끊김 → WARNING */
    ASSERT_EQ(1410u, s.transitions[1].t_ms);                      /* 마지막 수신 900 + 500ms 초과 = 첫 판단 1410ms */
    ASSERT_EQ(BCM_STATE_WARNING, bcm.state);
    ASSERT_EQ(BCM_STATE_WARNING, sink.frames[20].data[0]);        /* 마지막 0x400에도 WARNING이 실린다 */
}

static void test_no_tick_without_bcm(void) {
    /* BCM이 없으면(NULL) 시간만 흘러도 할 일이 없다: 세기만 하고 죽지 않는다 */
    CanFrame frames[1] = { frame2(0x100u, 8u, 0x10, 0x27) };
    uint32_t ts[1] = { 0u };
    ArraySource as = { frames, ts, 1u, 0u, 99u, 100u, 0u };
    CanSource src = array_source(&as);
    VehicleInputs in;
    FrameLoopStats s;
    vehicle_inputs_init(&in);
    ASSERT_EQ(0, frame_loop_run(&src, &in, NULL, NULL, 0u, &s));
    ASSERT_EQ(10, s.ticks);
    ASSERT_EQ(0, s.tx_frames);
}

static void test_null_args(void) {
    VehicleInputs in;
    FrameLoopStats s;
    CanSource empty = { NULL, "empty", NULL };                    /* read_frame이 없는 소스 */
    vehicle_inputs_init(&in);
    ASSERT_EQ(-1, frame_loop_run(NULL, &in, NULL, NULL, 0u, &s));
    ASSERT_EQ(-1, frame_loop_run(&empty, &in, NULL, NULL, 0u, &s));   /* NULL 함수를 부르면 죽는다 → 미리 막기 */
    ArraySource as = { NULL, NULL, 0u, 0u, 99u, 0u, 0u };
    CanSource src = array_source(&as);
    ASSERT_EQ(-1, frame_loop_run(&src, &in, NULL, NULL, 1u, &s));      /* 출력이 1개라면서 NULL */
}

int main(void) {
    printf("[test_frame_loop]\n");
    log_set_level(LOG_LEVEL_OFF);                  /* 테스트 중에는 로그를 안 찍는다 */
    RUN_TEST(test_reads_until_end);
    RUN_TEST(test_source_error_stops);
    RUN_TEST(test_runs_bcm);
    RUN_TEST(test_sinks_get_tx);
    RUN_TEST(test_sink_failure_keeps_running);
    RUN_TEST(test_silence_keeps_judging);
    RUN_TEST(test_no_tick_without_bcm);
    RUN_TEST(test_null_args);
    return TEST_SUMMARY();
}
