#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "bcm.h"
#include "cli.h"
#if defined(__linux__)
#include "can_dump.h"
#include "socket_sink.h"
#include "socket_source.h"
#endif
#include "dispatcher.h"
#include "log_reader.h"
#include "logger.h"
#include "signals.h"
#include "version.h"

/* 종료 코드: 0 = 성공, 1 = 실행 중 실패(파일을 못 엶 등), 2 = 사용법이 틀림 */
#define EXIT_OK     0
#define EXIT_FAIL   1
#define EXIT_USAGE  2

static void print_summary(const LogReadStats *stats, const VehicleInputs *in, const BcmContext *bcm) {
    char speed_str[32], door_str[64], ign_str[32];
    vehicle_speed_format(&in->speed, speed_str, sizeof(speed_str));
    door_status_format(&in->doors, door_str, sizeof(door_str));
    ign_status_format(&in->ign, ign_str, sizeof(ign_str));

    printf("\n결과\n");
    printf("  read %u, ok %u, error %u  (빈 줄/주석 %u)\n",
           stats->lines, stats->frames, stats->parse_errors, stats->skipped);
    printf("  디스패처: ok=%u ignored=%u error=%u\n",
           stats->dispatch.ok, stats->dispatch.ignored, stats->dispatch.error);
    printf("\n최종 차량 상태\n");
    printf("  차속 : %s\n", speed_str);
    printf("  도어 : %s\n", door_str);
    printf("  시동 : %s\n", ign_str);
    printf("\nBCM\n");
    printf("  최종 상태 : %s\n", bcm_state_name(bcm->state));
    printf("  상태 전이 : %u회, 도어 잠금 명령 : %u회\n", stats->bcm_transitions, stats->bcm_lock_cmds);
    printf("  0x400 송신 : %u프레임\n", stats->tx_frames);
    if (stats->tx_errors > 0u) {
        printf("  송신 실패  : %u회\n", stats->tx_errors);
    }
}

int main(int argc, char *argv[]) {
    CliOptions opts;
    switch (cli_parse(argc, argv, &opts, stderr)) {
        case CLI_OK:
            break;
        case CLI_HELP:
            cli_print_usage(stdout, argv[0]);
            return EXIT_OK;
        case CLI_VERSION:
            printf("can-bcm-simulator %s\n", BCM_SIM_VERSION);
            return EXIT_OK;
        case CLI_ERROR:
        default:
            fprintf(stderr, "도움말: %s --help\n", argv[0]);
            return EXIT_USAGE;
    }

    log_set_level(opts.log_level);

#if defined(__linux__)
    if (opts.dump) {                        /* 덤프 모드 (M5-4): BCM 없이 찍기만 */
        LOG_INFO("can-bcm-simulator %s 시작: %s 덤프 (로그 레벨 %s)",
                 BCM_SIM_VERSION, opts.iface, log_level_name(opts.log_level));
        return can_dump_run(opts.iface) == 0 ? EXIT_OK : EXIT_FAIL;
    }
#else
    if (opts.iface != NULL) {
        fprintf(stderr, "--iface는 Linux(SocketCAN)에서만 됩니다. VM에서 실행하세요: multipass shell can-vm\n");
        return EXIT_USAGE;
    }
#endif

    LOG_INFO("can-bcm-simulator %s 시작: %s (로그 레벨 %s)", BCM_SIM_VERSION,
             opts.file != NULL ? opts.file : opts.iface, log_level_name(opts.log_level));

    VehicleInputs in;
    BcmContext bcm;
    LogReadStats stats;
    vehicle_inputs_init(&in);
    bcm_init(&bcm);                         /* 처음 한 번 (프레임마다 부르면 상태를 잊는다) */

    FILE *tx_out = NULL;                    /* --out (M4-8) */
    if (opts.out_file != NULL) {
        tx_out = fopen(opts.out_file, "w");
        if (tx_out == NULL) {
            LOG_ERROR("출력 파일을 열 수 없습니다: %s (%s)", opts.out_file, strerror(errno));
            return EXIT_FAIL;
        }
    }

    /* 입력 소스만 다르고, 그 뒤(디스패처 → BCM → 0x400)는 같은 frame_loop_run()이다 (M5-5) */
    int result = -1;
    if (opts.file != NULL) {
        result = log_read_file(opts.file, &in, &bcm, tx_out, &stats);      /* FileSource */
    }
#if defined(__linux__)
    else {
        SocketSource ss;
        /* 수신 필터 (M5-7): 디스패처가 쓰는 ID만 커널에서 걸러 받는다. --no-filter면 전부 */
        const uint32_t *rx_ids = opts.no_filter ? NULL : DISPATCH_RX_IDS;
        if (socket_source_open(&ss, opts.iface, rx_ids, DISPATCH_RX_ID_COUNT) == 0) {
            CanSource src = socket_source_as_can_source(&ss);

            /* 0x400은 받은 소켓으로 그대로 송신 (M5-6). --out이 있으면 파일에도 */
            SocketSink tx_sock;
            FileSink tx_file;
            CanSink sinks[2];
            size_t sink_count = 0u;
            socket_sink_init(&tx_sock, ss.fd);
            sinks[sink_count++] = socket_sink_as_can_sink(&tx_sock);
            if (tx_out != NULL) {
                file_sink_init(&tx_file, tx_out);
                tx_file.iface = opts.iface;
                sinks[sink_count++] = file_sink_as_can_sink(&tx_file);
            }

            LOG_INFO("%s에서 받는 중, 0x400 송신 중... (Ctrl+C로 종료)", opts.iface);
            result = frame_loop_run(&src, &in, &bcm, sinks, sink_count, &stats);   /* 소켓은 끝이 없다: 오류일 때만 돌아온다 */
            socket_source_close(&ss);
        }
    }
#endif
    if (tx_out != NULL) {
        fclose(tx_out);
    }
    if (result != 0) {
        return EXIT_FAIL;
    }
    print_summary(&stats, &in, &bcm);
    if (opts.out_file != NULL) {
        printf("  → %s\n", opts.out_file);
    }
    return EXIT_OK;
}
