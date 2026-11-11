/* 덤프 모드 (M5-4, Linux 전용): CAN 소켓에서 받는 프레임을 해석해서 로그로 찍는다
 *
 *   ./build-linux/bcm_sim --iface vcan0 --dump
 *   [   1.234s] INFO  RX 0x100 [8] 10 27 00 00 00 00 00 00    → 차속 100.00 km/h
 *
 * candump과 비슷하지만, 우리 신호 정의서대로 "무슨 뜻인지"까지 보여 준다.
 * 끝낼 때는 Ctrl+C (안전한 종료는 M5-9)
 */
#ifndef CAN_DUMP_H
#define CAN_DUMP_H

/* ifname에서 받는 대로 찍는다. 끝나지 않는다(Ctrl+C까지).
 * 반환값: 소켓을 못 열거나 읽기 오류면 1 */
int can_dump_run(const char *ifname);

#endif
