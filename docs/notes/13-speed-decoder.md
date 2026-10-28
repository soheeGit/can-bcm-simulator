# 13. 차속 디코더 (M2-3)

> 코드: `include/signals.h`, `src/signals.c`, `tests/test_signals.c`
> 실행: `make && ./build/bcm_sim`, `make test`

## 1. 실행 결과

```
can-bcm-simulator v0.0.1
샘플 프레임: 0x100 [8] 10 27 00 00 00 00 00 00
차속: 100.00 km/h
```

## 2. 디코더란?

```
CanFrame (바이트)          decode_vehicle_speed()        VehicleSpeed (의미)
100#1027000000000000   ──────────────────────────→   { valid = true, centi_kmh = 10000 }
                                                       = 100.00 km/h
```

- `can_get_bits()`(M2-1)와 디코더의 차이 →

| | `can_get_bits()` | `decode_vehicle_speed()` |
| --- | --- | --- |
| 역할 | 비트를 꺼내는 **범용 도구** | 그 도구로 **"차속"이라는 의미**를 만든다 |
| 무슨 신호인지 아나? | 모른다 (숫자 start/length만 받음) | 안다 (0x100, start 0, 길이 16, ×0.01) |
| 하는 일 | 밀고 자르기 | 프레임 검사(ID, DLC) → `can_get_bits()` 호출 → 무효값 확인 → 단위 붙여 저장 |
| 결과 | raw 숫자 (`10000`) | 의미 있는 값 (`VehicleSpeed { valid, centi_kmh }`) |

- 웹으로 치면? → `can_get_bits()`는 JSON **파서**(문자열 → 값), 디코더는 파싱한 값을 **검증해서 DTO로 만드는 변환 코드**다. 예: `objectMapper.readTree()`로 꺼낸 값을 `@Valid` 검사 후 `SpeedDto`에 담는 서비스 로직

## 3. 메시지 ID 상수 (`signals.h`)

```c
#define MSG_ID_VEHICLE_SPEED  0x100u
#define MSG_ID_DOOR_STATUS    0x200u
...
```

- 상수로 만든 이유 → 
  - **이름이 뜻을 알려준다** (매직 넘버 방지). `if (f->id == 0x100)`보다 `if (f->id == MSG_ID_VEHICLE_SPEED)`가 무슨 검사인지 바로 보인다
  - **정의서가 바뀌면 한 곳만 고친다.** "실행 중에는" 안 바뀌지만, 정의서 버전이 올라가면 바뀔 수 있다 (예: 차속 ID를 0x0F0으로 변경). 숫자를 여러 파일에 흩어 쓰면 하나를 빼먹는다
- 차속 신호의 위치(`VEHICLE_SPEED_START_BIT`, `_LENGTH`)도 상수로 만든 이유 → 변하지 않는 값이라서 🔺 + **정의서의 표와 코드가 1:1로 대응**하게 하려고. 리뷰할 때 `signals.h`의 상수와 `docs/signal-spec.md`의 표만 비교하면 된다. `can_get_bits(f->data, 0, 16)`처럼 숫자만 쓰면 "0, 16이 뭐였더라?" 하고 정의서를 다시 찾아야 한다

## 4. 내부 표현: 정수 vs float — 설계 결정 (이슈 할 일)

```c
typedef struct {
    bool     valid;
    uint16_t centi_kmh;    /* 0.01 km/h 단위 */
} VehicleSpeed;
```

- `centi_kmh`의 뜻 (`centi` = 1/100) → **0.01 km/h 단위**로 센 차속. `centimeter`(1/100 m)의 centi와 같다. `centi_kmh = 10000` → 100.00 km/h. 정의서의 factor 0.01과 단위가 같아서 **raw 값을 그대로** 넣으면 된다 (변환 계산이 없음)
- float(`100.0f`)로 보관하지 않은 이유 (M1-3에서 본 것) →
  - **오차가 없다.** float는 `0.1 + 0.2 ≠ 0.3` 같은 오차가 있고, 계속 더하면 쌓인다. 정수는 정확하다 (금액을 센트 단위 정수로 저장하는 것과 같다)
  - **차량 MCU는 실수 계산이 느리거나 못 하는 경우가 많다** (FPU 없음)
  - **비교가 안전하다.** float는 `==` 비교를 믿을 수 없지만 정수는 된다
  - **크기가 작다.** `uint16_t` 2바이트 vs `float` 4바이트
- 그럼 "15 km/h 이상이면 잠금"(M4-4)은 어떻게 비교하나? → 기준값도 **같은 단위(0.01 km/h)로 바꿔서** 정수끼리 비교한다

```c
#define AUTO_LOCK_SPEED_CENTI_KMH  1500u          /* 15.00 km/h (M4-4 예정) */
if (speed.valid && speed.centi_kmh >= AUTO_LOCK_SPEED_CENTI_KMH) { /* 잠금 */ }
```

- 화면에 `100.00 km/h`로 찍을 때 float 없이 하는 방법 (`vehicle_speed_format`) → **나눗셈 몫과 나머지**로 정수 부분과 소수 부분을 따로 찍는다

```c
10000 / 100 = 100   → 정수 부분 "%u"
10000 % 100 = 0     → 소수 부분 "%02u" (2자리, 빈자리는 0) → "00"
5 / 100 = 0, 5 % 100 = 5 → "0.05" (%02u 덕분에 "0.5"가 아니라 "0.05")
```

## 5. 두 종류의 "실패"

| 상황 | 반환값 | `out->valid` | 의미 |
| --- | --- | --- | --- |
| 정상 (`10 27`) | `true` | `true` | 차속 100.00 km/h. 그대로 쓰면 된다 |
| 신호 무효 (`FF FF`) | `true` | `false` | 프레임은 정상인데, **송신 측(ESC)이 "차속 모름"이라고 보냄** |
| DLC가 1 | `false` | (안 건드림) | data[1]이 없어서 **해석 불가** |
| ID가 0x200 | `false` | (안 건드림) | 차속 메시지가 아님 → **잘못 호출** |
| NULL | `false` | (안 건드림) | 넘길 게 없음 → **잘못 호출** |

- "프레임을 해석할 수 없음"과 "신호가 무효"를 왜 나눴나? → **원인과 대응이 다르기 때문**
  - 해석 불가(`false`): 프레임이 깨졌거나 코드가 잘못 불렀다 → 이 프레임은 **버리고** 다음 프레임을 기다린다. 이전에 받은 차속은 그대로 둔다
  - 신호 무효(`valid = false`): 프레임은 멀쩡히 왔고, ESC가 "센서가 고장 나서 차속을 모른다"고 **정보를 준 것** → BCM은 "차속 모름" 상태로 바꾸고 **안전한 쪽으로** 판단해야 한다 (M4 Fail-safe)
  - 하나로 합치면 "프레임이 잠깐 깨진 것"과 "센서가 고장 난 것"을 구분할 수 없다
- `0xFFFF`를 받았을 때 그냥 0 km/h로 처리하면 무슨 일이? (M1-3) → BCM이 "**정차했다**"고 오해한다. 실제로는 달리고 있는데 자동 잠금을 **풀어버리거나**, 주행 중 문 열림 경고를 **안 울릴** 수 있다. "모름"과 "0"은 완전히 다른 정보다

## 6. 테스트 (이슈 완료 조건)

| 완료 조건 | 테스트 |
| --- | --- |
| 0 km/h | `test_speed_zero` (`00 00` → valid, 0) |
| 100.00 km/h | `test_speed_100kmh` (`10 27` → valid, 10000) |
| 최대값 | `test_speed_max` (`FE FF` = 0xFFFE → valid, 65534 = 655.34 km/h) |
| 무효값 | `test_speed_invalid_value` (`FF FF` → 반환 true, valid false) |
| (추가) 예약 바이트 무시 | `test_speed_ignores_reserved_bytes` (data[2~7]이 `FF`여도 10000) |
| (추가) 해석 불가 | `test_speed_dlc_too_short`, `test_speed_wrong_id`, `test_speed_invalid_frame`(DLC 9), `test_speed_null` → 전부 false |
| (추가) 최소 길이 | `test_speed_dlc_min_ok` (DLC 2 → 성공) |
| (추가) 문자열 | `test_speed_format` ("100.00 km/h", "0.05 km/h", "655.34 km/h", "INVALID") |

- 결과 → `[test_signals]` **24 passed**, 0 failed
- `test_speed_dlc_min_ok`가 확인하는 것 (M1-3 정의서의 "최소 길이") → DLC가 8이 아니라 **2여도** 받아주는지. 정의서에 "차속은 최소 2바이트"라고 정했으니 2바이트만 와도 해석해야 한다 ("받을 때는 너그럽게"). 반대로 1바이트면 거부하는 건 `test_speed_dlc_too_short`가 확인
- `test_speed_format`에서 `0.05 km/h`를 테스트한 이유 → 소수 부분이 한 자리 숫자(5)일 때 **앞에 0을 채우는지** 확인하려고. `%02u`를 `%u`로 잘못 쓰면 `0.5 km/h`가 찍혀서 **10배 틀린 값**처럼 보인다. 이런 실수는 100.00 같은 값으로는 안 드러나고 경계 쪽 값에서만 드러난다
- `speed_frame()` 도우미 함수를 만든 이유 → 테스트마다 `{ .id = MSG_ID_VEHICLE_SPEED, .dlc = 8, .data = {...} }`를 반복해서 쓰지 않으려고. 테스트에서 **중요한 값(data[0], data[1])만 눈에 띄게** 한다. JUnit에서 테스트 데이터를 만드는 `createXxx()` 헬퍼(테스트 픽스처)와 같다
