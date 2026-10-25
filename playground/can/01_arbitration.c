/* M1-1 실험: CAN 중재(arbitration) 시뮬레이터
 *
 * 여러 제어기가 "동시에" 메시지를 보내려고 하면 누가 이길까?
 *
 * CAN 버스의 규칙
 *   - 0 = dominant(우세), 1 = recessive(열세)
 *   - 누군가 0을 보내면 버스는 0이 된다 (0이 1을 이긴다) → 버스 = 모든 송신 비트의 AND
 *   - ID를 맨 위 비트(10번)부터 한 비트씩 보낸다
 *   - 1을 보냈는데 버스에서 0이 보이면 → "나보다 급한 애가 있구나" → 송신 포기(탈락)
 *   - 끝까지 남은 하나가 이긴다. 진 쪽은 기다렸다가 다음에 다시 보낸다
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define ID_BITS 11   /* Standard CAN ID = 11비트 */

typedef struct {
    const char *name;
    uint32_t id;
    bool active;     /* 아직 경쟁 중인가 */
} Node;

static void arbitrate(const char *title, Node *nodes, int count) {
    printf("===== %s\n", title);
    for (int i = 0; i < count; i++) {
        nodes[i].active = true;
        printf("  %-10s ID 0x%03X\n", nodes[i].name, nodes[i].id);
    }

    /* 표 머리 */
    printf("\n  bit |");
    for (int i = 0; i < count; i++) {
        printf(" %-10s|", nodes[i].name);
    }
    printf(" 버스\n");

    for (int bit = ID_BITS - 1; bit >= 0; bit--) {
        /* 1) 경쟁 중인 노드가 각자 비트를 보낸다. 버스 = 전부 AND */
        uint32_t bus = 1u;
        for (int i = 0; i < count; i++) {
            if (nodes[i].active) {
                bus &= (nodes[i].id >> bit) & 1u;
            }
        }

        printf("  %3d |", bit);
        for (int i = 0; i < count; i++) {
            if (nodes[i].active) {
                printf(" %-10u|", (nodes[i].id >> bit) & 1u);
            } else {
                printf(" %-10s|", "-");
            }
        }
        printf("  %u", bus);

        /* 2) 1을 보냈는데 버스가 0이면 탈락 */
        for (int i = 0; i < count; i++) {
            uint32_t sent = (nodes[i].id >> bit) & 1u;
            if (nodes[i].active && sent == 1u && bus == 0u) {
                nodes[i].active = false;
                printf("   ← %s 탈락", nodes[i].name);
            }
        }
        printf("\n");
    }

    for (int i = 0; i < count; i++) {
        if (nodes[i].active) {
            printf("\n  🏆 승리: %s (ID 0x%03X)\n\n", nodes[i].name, nodes[i].id);
        }
    }
}

int main(void) {
    /* 시나리오 1: 신호 정의서의 세 메시지가 동시에 송신 */
    Node s1[] = {
        {"DOOR",  0x200, true},
        {"SPEED", 0x100, true},
        {"IGN",   0x300, true},
    };
    arbitrate("시나리오 1: 0x200 vs 0x100 vs 0x300", s1, 3);

    /* 시나리오 2: SPEED가 빠지면 DOOR와 IGN 중 누가 이길까? */
    Node s2[] = {
        {"IGN",  0x300, true},
        {"DOOR", 0x200, true},
    };
    arbitrate("시나리오 2: 0x300 vs 0x200", s2, 2);

    /* 시나리오 3: ID가 딱 1 차이 */
    Node s3[] = {
        {"A", 0x101, true},
        {"B", 0x100, true},
    };
    arbitrate("시나리오 3: 0x101 vs 0x100", s3, 2);

    return 0;
}
