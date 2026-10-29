# 시나리오 로그 (M3-4)

BCM 상태 머신(M4)을 검증하기 위한 주행 시나리오. **TDD의 인수 테스트**처럼, 여기 적은 "기대 결과"를 M4가 만족해야 한다.

- 형식: `docs/log-format.md` (candump `-l`). **주석 없음** (M3-1 결정)
- 생성: `make scenarios` (`tools/gen_scenarios.c`가 `can_set_bits()`로 만든다. 손으로 고치지 말고 생성기를 고칠 것)
- 실행: `./build/bcm_sim --file data/scenarios/01_normal_drive.log`
- 모든 메시지는 100ms 주기 (0x300 → 0x200 → 0x100 순서). 시각은 0초부터

> 아래 "기대 BCM 동작"은 [docs/fsm-design.md](../../docs/fsm-design.md)의 상태 전이 표(T1~T11) 기준이다 (M4-1에서 확정).
> 요약: IDLE → **DRIVING_LOCKED**: 시동 ON + 15 km/h 이상 + 문 전부 닫힘 / DRIVING_LOCKED → **WARNING**: 문 열림 + 3 km/h 초과, 또는 신호 이상(Fail-safe) / WARNING → 문 전부 닫히면 원래 상태로 / → **IDLE**: 0 km/h + P단, 또는 시동 OFF/ACC

## 01_normal_drive.log — 정상 주행

| 시각 | 상황 |
| --- | --- |
| 0.0초 | 시동 OFF, P, 0 km/h, 문 닫힘 |
| 0.5초 | 시동 ON |
| 1.0초 | 기어 D, 가속 시작 (100ms마다 +2 km/h) |
| 1.8초 | 16 km/h (처음으로 15 km/h 이상) |
| 4.0 ~ 6.0초 | 60 km/h 정속 |
| 6.0 ~ 9.0초 | 감속 (100ms마다 -2 km/h) → 9.0초 0 km/h |
| 9.5초 | 기어 P |
| 10.0초 | 시동 OFF (10.5초까지 기록) |

- 줄 수: 318 (106시점 × 3메시지), 오류 0
- **기대 BCM 동작:** IDLE → **1.8초 DRIVING_LOCKED** (도어 잠금 명령 1회) → **9.5초 IDLE**. 경고음 없음

## 02_door_open_while_driving.log — 주행 중 운전석 문 열림

| 시각 | 상황 |
| --- | --- |
| 0.5초 / 1.0초 | 시동 ON / 기어 D, 가속 시작 (100ms마다 +2 km/h) |
| 1.8초 | 16 km/h |
| 3.0 ~ 7.0초 | 40 km/h 정속 |
| **4.0 ~ 4.9초** | **운전석(FL) 문 열림** (`200#01`) |
| 5.0초 | 문 닫힘 |
| 7.0 ~ 9.0초 | 감속 → 9.0초 0 km/h |
| 9.5초 / 10.0초 | P / 시동 OFF |

- 줄 수: 318, 오류 0
- **기대 BCM 동작:** 1.8초 DRIVING_LOCKED → **4.0초 WARNING (경고음 ON)** → **5.0초 DRIVING_LOCKED (경고음 OFF)** → 9.5초 IDLE
- 문 열림 신호에 노이즈가 없어서 디바운싱(M4-5, 3회 연속 확인)을 넣으면 WARNING 진입이 **4.2초**로 늦어진다

## 03_signal_timeout.log — 주행 중 차속 신호 끊김

| 시각 | 상황 |
| --- | --- |
| 0.5초 / 1.0초 | 시동 ON / 기어 D, 가속 시작 (100ms마다 +5 km/h) |
| 1.3초 | 15 km/h |
| 2.0 ~ 5.0초 | 50 km/h (실제로 달리는 중) |
| 2.9초 | 마지막 차속 메시지 |
| **3.0 ~ 3.9초** | **0x100(차속)만 안 옴** (0x300, 0x200은 계속 옴) |
| 4.0초 | 차속 메시지 다시 옴 (50 km/h) |
| 5.0 ~ 7.0초 | 감속 → 7.0초 0 km/h |
| 7.5초 / 8.0초 | P / 시동 OFF |

- 줄 수: 248 (차속 10줄 빠짐), 오류 0
- **기대 BCM 동작:** 1.3초 DRIVING_LOCKED → 마지막 수신(2.9초)에서 500ms를 넘긴 **3.5초에 타임아웃 감지 → WARNING (Fail-safe)** → **4.0초 차속 복귀 → DRIVING_LOCKED** → 7.5초 IDLE
- 핵심: 마지막 차속(50 km/h)을 계속 믿으면 안 된다. "모름"이 됐다는 걸 알아채야 한다 (M4-6)

## 04_invalid_frames.log — 이상한 프레임 섞임 (파일 모드 전용)

30 km/h로 짧게 주행하면서 중간에 이상한 줄과 이상한 값을 끼워 넣었다.

| 시각 | 끼워 넣은 것 | 기대 처리 |
| --- | --- | --- |
| 1.51초 | `555#AABBCCDD` | 관심 없는 ID → 디스패처 IGNORED |
| 1.61초 | `100#10` (차속인데 DLC 1) | 파싱 OK → 디스패처 ERROR, 이전 차속 유지 |
| 1.71초 | `100#10G7...` | 파서 ERR_DATA |
| **1.8초** | **차속 `FFFF` (무효값)** | 디스패처 OK, `speed.valid = false` → BCM은 "차속 모름" |
| 1.91초 | `100#102` | 파서 ERR_DATA |
| 2.01초 | `800#00` (ID > 0x7FF) | 파싱 OK → 디스패처 ERROR (`can_frame_is_valid`) |
| 2.11초 | `100#R` | 파서 ERR_UNSUPPORTED |
| **2.2초** | **기어 `7` (무효값)** | 디스패처 OK, `gear = GEAR_INVALID` |
| 2.3초 근처 | `garbage line` | 파서 ERR_TIMESTAMP |
| 2.41초 | `400#01010000...` (우리 BCM 메시지) | 디스패처 IGNORED |

- 줄 수: 137 (정상 129 + 끼워 넣은 8), 실행 결과: `read 137, ok 133, error 4`, 디스패처 `ok=129 ignored=2 error=2`
- **기대 BCM 동작:** 0.8초 DRIVING_LOCKED → **1.8초 WARNING** (차속 무효 = 신호 이상, Fail-safe) → **1.9초 DRIVING_LOCKED** (정상 차속 복귀). 2.2초 기어 무효는 상태 변화 없음 (기어는 정차 후 IDLE 복귀 판단에만 쓴다). 깨진 줄로는 상태가 바뀌지 않는다
- ⚠️ can-utils(`log2long`, `canplayer`)는 첫 깨진 줄에서 멈춘다 → **M5 재생에는 01~03만** 쓴다

## 정리

| 파일 | 줄 수 | 형식 오류 | M5 재생 | 검증 포인트 |
| --- | --- | --- | --- | --- |
| 01_normal_drive | 318 | 0 | ✅ | 자동 잠금, 정차 후 IDLE |
| 02_door_open_while_driving | 318 | 0 | ✅ | 주행 중 문 열림 경고 |
| 03_signal_timeout | 248 | 0 | ✅ | 신호 끊김 Fail-safe |
| 04_invalid_frames | 137 | 4 | ❌ | 이상한 입력에 안 죽기, 무효값 처리 |
