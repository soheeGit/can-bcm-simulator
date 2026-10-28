# can-bcm-simulator
[미니 프로젝트] 가상 CAN 패킷 기반 차량 상태 제어 시뮬레이터 (C언어)

candump 형식의 CAN 로그 파일을 읽어 차속/도어/시동 신호를 해석하고, 차량 상태를 로그로 보여 준다.
(BCM 제어 로직은 M4, 실제 vcan 소켓은 M5에서 추가 예정)

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
| `--help` | `-h` | 사용법 |
| `--version` | `-V` | 버전 |

종료 코드: `0` 성공, `1` 실행 실패(파일을 열 수 없음 등), `2` 옵션이 잘못됨

### 예: 주행 중 도어 열림

```bash
./build/bcm_sim --file data/scenarios/02_door_open_while_driving.log
```

```
[   0.000s] INFO  can-bcm-simulator 0.1.0 시작: data/scenarios/02_door_open_while_driving.log (로그 레벨 INFO)
[   0.000s] INFO  시동  IGN=OFF GEAR=P
[   0.000s] INFO  도어  FL=0 FR=0 RL=0 RR=0 TRUNK=0
[   0.000s] INFO  차속  0.00 km/h
...
[   4.000s] INFO  도어  FL=1 FR=0 RL=0 RR=0 TRUNK=0
[   5.000s] INFO  도어  FL=0 FR=0 RL=0 RR=0 TRUNK=0
...

결과
  read 318, ok 318, error 0  (빈 줄/주석 0)
  디스패처: ok=318 ignored=0 error=0
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
