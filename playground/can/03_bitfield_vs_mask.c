/* M1-4 실험 2: CAN 데이터를 비트필드로 "그냥 덮어쓰면" 될까?
 *
 * 방법 A: 비트필드 구조체를 만들고, 받은 바이트를 memcpy로 덮어쓰기
 * 방법 B: 마스크/시프트로 정의서의 비트 위치에서 직접 꺼내기 (M0-5)
 *
 * 정의서(docs/signal-spec.md) 기준으로 두 방법의 결과를 비교한다.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---------- 0x200 DOOR_STATUS ---------- */

typedef struct {
    uint8_t fl    : 1;   /* 정의서: bit 0 */
    uint8_t fr    : 1;   /* bit 1 */
    uint8_t rl    : 1;   /* bit 2 */
    uint8_t rr    : 1;   /* bit 3 */
    uint8_t trunk : 1;   /* bit 4 */
    uint8_t       : 3;   /* 이름 없는 비트필드 = 예약 비트 자리 채우기 */
} DoorBits;

/* ---------- 0x300 IGN_STATUS ---------- */

typedef struct {
    uint8_t ign  : 2;    /* 정의서: bit 0~1 */
    uint8_t      : 6;    /* 예약 (bit 2~7) */
    uint8_t gear : 3;    /* 정의서: bit 8~10 */
    uint8_t      : 5;    /* 예약 */
} IgnBits;

/* ---------- 바이트 경계를 넘는 신호: start bit 4, 길이 8 ---------- */
/* 정의서 방식이라면 bit 4~11 = data[0]의 위 4비트 + data[1]의 아래 4비트 */

typedef struct {
    uint8_t low  : 4;    /* bit 0~3 */
    uint8_t sig  : 8;    /* 의도: bit 4~11 */
} CrossBits;

static void door_by_mask(const uint8_t *data, unsigned out[5]) {
    for (unsigned n = 0; n < 5; n++) {
        out[n] = (data[0] >> n) & 1u;
    }
}

int main(void) {
    /* (1) 도어: 정의서 예시 3 (200#11...) → 운전석 + 트렁크 열림 */
    uint8_t door_data[8] = {0x11};
    DoorBits db;
    memcpy(&db, door_data, sizeof(db));
    unsigned dm[5];
    door_by_mask(door_data, dm);
    printf("[1] DOOR 0x11  비트필드 : FL=%u FR=%u RL=%u RR=%u TRUNK=%u\n",
           db.fl, db.fr, db.rl, db.rr, db.trunk);
    printf("[1] DOOR 0x11  마스크   : FL=%u FR=%u RL=%u RR=%u TRUNK=%u\n",
           dm[0], dm[1], dm[2], dm[3], dm[4]);

    /* (2) 시동/기어: 정의서 예시 4 (300#0203...) → ON, D */
    uint8_t ign_data[8] = {0x02, 0x03};
    IgnBits ib;
    memcpy(&ib, ign_data, sizeof(ib));
    printf("[2] IGN 02 03  비트필드 : ign=%u gear=%u\n", ib.ign, ib.gear);
    printf("[2] IGN 02 03  마스크   : ign=%u gear=%u\n",
           ign_data[0] & 0x03u, ign_data[1] & 0x07u);

    /* (3) 바이트 경계를 넘는 신호 */
    uint8_t cross_data[8] = {0xA0, 0x0B};   /* bit 4~11 = 0xBA 가 되도록 넣은 데이터 */
    CrossBits cb;
    memset(&cb, 0, sizeof(cb));
    memcpy(&cb, cross_data, sizeof(cb) < 8 ? sizeof(cb) : 8);
    uint32_t word = (uint32_t)cross_data[0] | ((uint32_t)cross_data[1] << 8);
    printf("[3] sizeof(CrossBits)   : %zu\n", sizeof(CrossBits));
    printf("[3] CROSS      비트필드 : sig=0x%02X\n", cb.sig);
    printf("[3] CROSS      마스크   : sig=0x%02X\n", (word >> 4) & 0xFFu);

    /* (4) 이 컴퓨터의 정보 */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    printf("[4] 이 컴퓨터: Little-endian\n");
#else
    printf("[4] 이 컴퓨터: Big-endian\n");
#endif

    return 0;
}
