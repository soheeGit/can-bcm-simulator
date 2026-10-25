# 신호 정의서 (Signal Specification)

> 이 프로젝트에서 주고받는 CAN 메시지와 신호의 정의. 실무의 **DBC 파일**을 축소한 문서다.
> 디코더(M2), 로그 시나리오(M3), BCM 상태 머신(M4)은 모두 이 문서를 기준으로 한다.
> 웹으로 치면 **API 스펙 문서(OpenAPI)**에 해당한다.

## 1. 공통 규칙

| 항목 | 규칙 |
| --- | --- |
| CAN 종류 | 일반 CAN (Classic CAN), Data Frame만 사용 |
| ID | Standard ID (11비트, `0x000` ~ `0x7FF`) |
| DLC | 모든 메시지 **8**. 단, 디코더는 아래 "최소 길이"보다 짧으면 버린다 |
| 바이트 순서 | **Intel (Little-endian)**: 여러 바이트에 걸친 신호는 앞 바이트(`data[0]`)가 작은 자리 |
| 비트 번호 | 8바이트 전체를 이어 붙여 **0 ~ 63번**. 비트 `n`은 `data[n / 8]`의 `n % 8`번째 비트 |
| 물리값 변환 | **물리값 = raw × factor + offset** |
| 예약(Reserved) 비트 | 정의되지 않은 비트. 송신 측은 0으로 보내고, 수신 측은 무시한다 |
| 송신 주기 | 모든 메시지 **100ms** |
| 타임아웃 | 수신 측은 **500ms(주기의 5배)** 동안 메시지를 못 받으면 해당 신호를 "끊김"으로 판단한다 (M4-6) |

### 비트 번호 그림

```
          bit 7  6  5  4  3  2  1  0
data[0]:      7  6  5  4  3  2  1  0
data[1]:     15 14 13 12 11 10  9  8
data[2]:     23 22 21 20 19 18 17 16
data[3]:     31 30 29 28 27 26 25 24
data[4]:     39 38 37 36 35 34 33 32
data[5]:     47 46 45 44 43 42 41 40
data[6]:     55 54 53 52 51 50 49 48
data[7]:     63 62 61 60 59 58 57 56
```

## 2. 메시지 목록

| ID | 메시지 이름 | 송신 | 수신 | 주기 | DLC | 최소 길이 |
| --- | --- | --- | --- | --- | --- | --- |
| `0x100` | VEHICLE_SPEED | ESC (브레이크 제어기) | BCM, 계기판 | 100ms | 8 | 2바이트 |
| `0x200` | DOOR_STATUS | 도어 모듈 | BCM | 100ms | 8 | 1바이트 |
| `0x300` | IGN_STATUS | 스마트키 제어기 | BCM | 100ms | 8 | 2바이트 |
| `0x400` | BCM_STATUS | **BCM (이 프로젝트)** | 계기판 등 | 100ms | 8 | 8바이트 |

ID 순서 = 우선순위 순서 (작을수록 먼저, M1-1 중재). 차속이 가장 급하다.

## 3. 신호 상세

### 0x100 VEHICLE_SPEED

| 신호 | start bit | 길이 | 부호 | factor | offset | 범위 (물리값) | 단위 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| VehicleSpeed | 0 | 16 | unsigned | 0.01 | 0 | 0 ~ 655.34 | km/h |

- raw `0xFFFF` = **신호 무효** (센서 고장 등). 물리값으로 쓰지 않는다

```
          bit 7  6  5  4  3  2  1  0
data[0]:  [   VehicleSpeed 하위 8비트 (bit 0~7)  ]
data[1]:  [   VehicleSpeed 상위 8비트 (bit 8~15) ]
data[2~7]: 예약
```

### 0x200 DOOR_STATUS

| 신호 | start bit | 길이 | 값 |
| --- | --- | --- | --- |
| DoorFL (운전석) | 0 | 1 | 0 = 닫힘, 1 = 열림 |
| DoorFR (조수석) | 1 | 1 | 0 = 닫힘, 1 = 열림 |
| DoorRL (뒷좌석 왼쪽) | 2 | 1 | 0 = 닫힘, 1 = 열림 |
| DoorRR (뒷좌석 오른쪽) | 3 | 1 | 0 = 닫힘, 1 = 열림 |
| Trunk (트렁크) | 4 | 1 | 0 = 닫힘, 1 = 열림 |

```
          bit 7  6  5  4      3     2     1     0
data[0]:  [ 예약  ] Trunk  DoorRR DoorRL DoorFR DoorFL
data[1~7]: 예약
```

### 0x300 IGN_STATUS

| 신호 | start bit | 길이 | 값 |
| --- | --- | --- | --- |
| IgnState | 0 | 2 | 0 = OFF, 1 = ACC, 2 = ON, 3 = START |
| Gear | 8 | 3 | 0 = P, 1 = R, 2 = N, 3 = D, 4 ~ 7 = **무효** |

```
          bit 7  6  5  4  3  2  1  0
data[0]:  [     예약        ][IgnState]
data[1]:  [   예약     ][   Gear     ]
data[2~7]: 예약
```

### 0x400 BCM_STATUS (BCM이 송신)

| 신호 | start bit | 길이 | 값 |
| --- | --- | --- | --- |
| BcmState | 0 | 2 | 0 = IDLE, 1 = DRIVING_LOCKED, 2 = WARNING, 3 = 예약 |
| DoorLockCmd | 8 | 1 | 0 = 없음, 1 = 도어 잠금 명령 |
| WarningBuzzer | 9 | 1 | 0 = OFF, 1 = 경고음 ON |
| AliveCounter | 56 | 4 | 0 ~ 15, 송신할 때마다 1씩 증가 (15 다음 0) |

- AliveCounter: 수신 측은 값이 계속 바뀌는지 보고 BCM이 살아 있는지 판단한다

```
          bit 7  6  5  4  3  2      1             0
data[0]:  [        예약        ][   BcmState      ]
data[1]:  [        예약        ] WarningBuzzer DoorLockCmd
data[2~6]: 예약
data[7]:  [    예약     ][     AliveCounter (bit 56~59)     ]
```

## 4. 예시 프레임과 해석

로그 표기는 candump 형식 `ID#DATA` (M3-1)

### 예시 1: 차속 100.00 km/h

```
100#1027000000000000
data[0] = 0x10, data[1] = 0x27
raw = data[0] | (data[1] << 8) = 0x2710 = 10000      ← Little-endian
물리값 = 10000 × 0.01 + 0 = 100.00 km/h
```

### 예시 2: 차속 신호 무효

```
100#FFFF000000000000
raw = 0xFFFF → 신호 무효 (물리값 계산하지 않음)
```

### 예시 3: 운전석 + 트렁크 열림

```
200#1100000000000000
data[0] = 0x11 = 0001 0001
bit 0 (DoorFL) = 1 → 열림
bit 4 (Trunk)  = 1 → 열림
나머지 = 0 → 닫힘
```

### 예시 4: 시동 ON, 기어 D

```
300#0203000000000000
IgnState = bit 0~1 = data[0] & 0x03 = 2 → ON
Gear     = bit 8~10 = data[1] & 0x07 = 3 → D
```

### 예시 5: BCM 상태 — 주행 잠금, 잠금 명령, AliveCounter 5

```
400#0101000000000005
BcmState      = data[0] & 0x03        = 1 → DRIVING_LOCKED
DoorLockCmd   = data[1] & 0x01        = 1 → 잠금 명령
WarningBuzzer = (data[1] >> 1) & 0x01 = 0 → OFF
AliveCounter  = data[7] & 0x0F        = 5
```

## 부록: 실제 DBC 파일로 쓰면

실무에서는 이 내용을 Vector CANdb++ 같은 도구로 `.dbc` 파일에 저장한다. 같은 내용을 DBC 문법으로 쓰면:

```
BO_ 256 VEHICLE_SPEED: 8 ESC
 SG_ VehicleSpeed : 0|16@1+ (0.01,0) [0|655.34] "km/h" BCM,CLUSTER

BO_ 512 DOOR_STATUS: 8 DOOR
 SG_ DoorFL : 0|1@1+ (1,0) [0|1] "" BCM
 SG_ DoorFR : 1|1@1+ (1,0) [0|1] "" BCM
 SG_ DoorRL : 2|1@1+ (1,0) [0|1] "" BCM
 SG_ DoorRR : 3|1@1+ (1,0) [0|1] "" BCM
 SG_ Trunk  : 4|1@1+ (1,0) [0|1] "" BCM
```

- `BO_ 256` → 메시지 ID (10진수, 256 = 0x100), `8` → DLC, `ESC` → 송신자
- `SG_ 이름 : 0|16@1+` → start bit 0, 길이 16, `@1` = Intel(Little-endian), `+` = unsigned
- `(0.01,0)` → (factor, offset), `[0|655.34]` → 범위, `"km/h"` → 단위, 마지막은 수신자

## 변경 이력

| 날짜 | 내용 |
| --- | --- |
| M1-3 | 최초 작성: 0x100, 0x200, 0x300, 0x400 |
