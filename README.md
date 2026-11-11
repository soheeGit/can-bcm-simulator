# can-bcm-simulator
[미니 프로젝트] 가상 CAN 패킷 기반 차량 상태 제어 시뮬레이터 (C언어)

candump 형식의 CAN 로그 파일을 읽어 차속/도어/시동 신호를 해석하고, BCM 상태 머신이 자동 잠금 / 주행 중 도어 열림 경고 / 신호 끊김 Fail-safe를 판단한다.
(상태 전이 규칙: [docs/fsm-design.md](docs/fsm-design.md). 실제 vcan 소켓은 M5에서 추가 예정)

## 빌드 & 테스트

```bash
make              # 빌드 → build/bcm_sim
make test         # 단위 테스트 실행 (실패 시 종료 코드 0이 아님)
make debug        # AddressSanitizer + UBSan 포함 빌드 → build/debug/bcm_sim
make scenarios    # 테스트 시나리오 로그 다시 생성 → data/scenarios/
make clean
```

## 실행

```bash
./build/bcm_sim --file <로그 파일> [--log-level debug|info|warn|error|off]
```

| 옵션 | 짧게 | 설명 |
| --- | --- | --- |
| `--file <경로>` | `-f` | 읽을 candump 로그 파일 (필수) |
| `--log-level <레벨>` | `-l` | `debug` / `info`(기본) / `warn` / `error` / `off` |
| `--out <경로>` | `-o` | BCM이 100ms마다 보내는 0x400 BCM_STATUS를 candump 형식으로 저장 |
| `--help` | `-h` | 사용법 |
| `--version` | `-V` | 버전 |

종료 코드: `0` 성공, `1` 실행 실패(파일을 열 수 없음 등), `2` 옵션이 잘못됨

### 예: 주행 중 도어 열림

```bash
./build/bcm_sim --file data/scenarios/02_door_open_while_driving.log
```

```
[   0.000s] INFO  can-bcm-simulator 0.2.0 시작: data/scenarios/02_door_open_while_driving.log (로그 레벨 INFO)
[   0.000s] INFO  시동  IGN=OFF GEAR=P
[   0.000s] INFO  도어  FL=0 FR=0 RL=0 RR=0 TRUNK=0
[   0.000s] INFO  차속  0.00 km/h
...
[   1.800s] INFO  차속  16.00 km/h
[   1.800s] INFO  STATE IDLE -> DRIVING_LOCKED (T3: 시동 ON, 15km/h 이상, 문 닫힘)
[   1.800s] INFO  도어 잠금 명령
...
[   4.000s] INFO  도어  FL=1 FR=0 RL=0 RR=0 TRUNK=0
[   4.200s] INFO  STATE DRIVING_LOCKED -> WARNING (T6: 문 열림, 3km/h 초과)
[   5.000s] INFO  도어  FL=0 FR=0 RL=0 RR=0 TRUNK=0
[   5.200s] INFO  STATE WARNING -> DRIVING_LOCKED (T9: 문 닫힘, 잠금 상태로 복귀)
...
[   9.500s] INFO  STATE DRIVING_LOCKED -> IDLE (T7: 정차, P단)
...

결과
  read 318, ok 318, error 0  (빈 줄/주석 0)
  디스패처: ok=318 ignored=0 error=0
...
BCM
  최종 상태 : IDLE
  상태 전이 : 4회, 도어 잠금 명령 : 1회
  0x400 송신 : 106프레임
```

### 예: BCM 송신 프레임(0x400) 저장

```bash
./build/bcm_sim -f data/scenarios/02_door_open_while_driving.log -o result.log
```

```
(1.800000) vcan0 400#0000000000000002     ← IDLE, Alive Counter 2
(1.900000) vcan0 400#0101000000000003     ← DRIVING_LOCKED + 도어 잠금 명령
(2.000000) vcan0 400#0100000000000004
...
(4.300000) vcan0 400#020200000000000B     ← WARNING + 경고음
```

`data[0]` = 상태, `data[1]` = 잠금 명령(bit 0) / 경고음(bit 1), `data[7]` = Alive Counter (0 ~ 15). 정의: [docs/signal-spec.md](docs/signal-spec.md)

### 예: vcan0에서 실시간으로 받기 (Linux, M5)

SocketCAN은 Linux에만 있어서 VM에서 실행한다 (`./scripts/vm_setup.sh` → `multipass shell can-vm`). Linux에서는 빌드 폴더가 `build-linux/`다.

```bash
./scripts/setup_vcan.sh                              # vcan0 만들기
make && ./build-linux/bcm_sim --iface vcan0 --dump   # 받는 대로 해석해서 출력 (Ctrl+C로 종료)
cansend vcan0 100#1027000000000000                   # (다른 터미널) 보내 보기
```

```
[   0.525s] INFO  RX 0x100 [8] 10 27 00 00 00 00 00 00    → 차속 100.00 km/h
```

### 예: 잘못된 프레임만 보기

```bash
./build/bcm_sim -f data/scenarios/04_invalid_frames.log -l warn
```

```
[   1.610s] WARN  해석할 수 없는 프레임: 0x100 [1] 10
[   1.700s] WARN  줄 57: ERR_DATA: (1.710000) vcan0 100#10G7000000000000
...
```

시나리오 설명: [data/scenarios/README.md](data/scenarios/README.md) · 로그 형식: [docs/log-format.md](docs/log-format.md) · 신호 정의: [docs/signal-spec.md](docs/signal-spec.md)

학습 로드맵: [docs/ROADMAP.md](docs/ROADMAP.md)
