#include "log_reader.h"

#include <errno.h>
#include <string.h>

#include "logger.h"

int log_read_stream(FILE *in, VehicleInputs *inputs, BcmContext *bcm, FILE *tx_out, LogReadStats *stats) {
    if (in == NULL || inputs == NULL || stats == NULL) {
        return -1;
    }
    FileSource fs;
    file_source_init(&fs, in);
    CanSource src = file_source_as_can_source(&fs);     /* 파일을 "프레임 소스"로 감싸서 */

    int result = frame_loop_run(&src, inputs, bcm, tx_out, stats);   /* 소켓과 같은 루프로 */

    /* 줄 단위 통계는 파일 소스만 아는 것 → 옮겨 적는다 */
    stats->lines = fs.lines;
    stats->skipped = fs.skipped;
    stats->parse_errors = fs.parse_errors;
    return result;
}
int log_read_file(const char *path, VehicleInputs *inputs, BcmContext *bcm, FILE *tx_out, LogReadStats *stats) {
    int result = -1;
    FILE *in = NULL;

    if (path == NULL) {
        goto cleanup;
    }
    in = fopen(path, "r");
    if (in == NULL) {
        LOG_ERROR("파일을 열 수 없습니다: %s (%s)", path, strerror(errno));
        goto cleanup;
    }

    result = log_read_stream(in, inputs, bcm, tx_out, stats);

cleanup:
    /* C에는 try-finally가 없다 → 어디서 실패하든 여기 한 곳으로 모여서 정리한다 */
    if (in != NULL) {
        fclose(in);
    }
    return result;
}
