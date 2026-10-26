# 14. 도어 상태 디코더 (M2-4)

> 코드: `include/signals.h`, `src/signals.c`, `tests/test_signals.c`
> 실행: `make test`

## 1. 차속 디코더(M2-3)와 비교

| | 차속 `decode_vehicle_speed` | 도어 `decode_door_status` |
| --- | --- | --- |
| 메시지 ID | 0x100 | 0x200 |
| 최소 길이 | 2바이트 | 1바이트 |
| 신호 개수 | 1 (VehicleSpeed) | 5 (FL, FR, RL, RR, Trunk) |
| 신호 크기 | 16비트 (바이트 2개에 걸침) | 1비트씩 (전부 data[0] 안) |
| 결과 구조체 | `VehicleSpeed { bool valid; uint16_t centi_kmh; }` | `DoorStatus { bool fl, fr, rl, rr, trunk; }` |
| 무효값 | `0xFFFF` = 신호 무효 | 없음 |

- 두 디코더의 공통 순서 (검사 → 꺼내기 → 담기) →
  1. **검사:** NULL인가? 유효한 프레임인가(`can_frame_is_valid`)? 내 ID가 맞나? 최소 길이를 넘나? → 하나라도 아니면 `false`
  2. **꺼내기:** 정의서의 start bit, 길이로 `can_get_bits()` 호출
  3. **담기:** 꺼낸 raw 값을 의미 있는 형태(정수 단위, bool)로 바꿔 `out`에 넣고 `true`
- 차속에는 `valid`가 있는데 도어에는 없는 이유 → 1비트는 0과 1뿐이고 둘 다 뜻이 정해져 있어서 "무효"를 표시할 남는 값이 없다. 차속은 16비트(65536가지) 중 하나(`0xFFFF`)를 "무효"로 쓸 여유가 있다. 정의서에도 도어 무효값은 정의하지 않았다

## 2. `DoorStatus`: ADR-001 적용

```c
typedef struct {
    bool fl, fr, rl, rr, trunk;
} DoorStatus;
```

- M1-4 ADR-001의 어떤 결정을 적용한 것인가? → 결정 2 **"내부 상태는 비트필드가 아니라 일반 구조체(bool)로 표현"**. CAN 데이터는 `can_get_bits()`(마스크)로 꺼내고, 꺼낸 결과를 bool에 담는다
- 비트필드 `uint8_t fl : 1;`로 하지 않은 이유 (한 줄로) → 아낄 메모리가 몇 바이트뿐이라, 읽기 쉽고 디버거로 보기 쉽고 주소도 구할 수 있는 bool이 낫다
- `bool`의 크기는? `sizeof(DoorStatus)`는 대략? → `bool`은 **1바이트**, `sizeof(DoorStatus)` = **5** (bool 5개 × 1바이트, 전부 1바이트짜리라 패딩 없음). 비트필드로 했다면 1바이트였으니 4바이트를 더 쓰는 셈이지만, 그 정도는 차이가 없다

## 3. `get_flag()` 도우미

```c
static bool get_flag(const uint8_t data[8], uint8_t bit) {
    return can_get_bits(data, bit, DOOR_SIGNAL_LENGTH) != 0u;
}
```

- `!= 0u`로 bool을 만드는 이유 → `can_get_bits()`는 **숫자(`uint32_t`)**를 돌려주고, 이 함수는 **참/거짓(`bool`)**을 돌려줘야 한다. `!= 0u` 비교를 하면 결과가 바로 true/false라서 **"숫자를 참/거짓으로 바꾼다"는 의도가 코드에 드러난다.** (그냥 `return can_get_bits(...);`로 써도 C에서는 0이 아니면 true로 바뀌지만, 숫자와 bool을 섞어 쓰는 건 MISRA-C 같은 코딩 규칙에서 피하라고 한다)
- `static`인 이유 → 이 파일 안에서만 사용하기 때문이다. ✅ (Java의 `private`) 덧붙이면: 헤더에 안 나오니 밖에서는 존재를 모르고, `get_flag` 같은 흔한 이름이 다른 파일과 **충돌하지 않는다** (M0-7)

## 4. `door_any_open()` — 트렁크를 포함할까? (설계 결정)

- 이 함수는 어디에 쓰나? (M4) → 주행중 문이 열려있을때 경고음 ✅ M4 상태 머신에서 두 군데:
  - `DRIVING_LOCKED` 상태에서 `door_any_open() && 속도 > 3 km/h` → **WARNING** (경고음 ON)
  - `IDLE`에서 출발할 때 문이 열려 있으면 → 자동 잠금 대신 경고 (M4-1 전이 표)
- 트렁크를 포함한 이유 → **트렁크가 열린 채 달리는 것도 위험**하기 때문이다 (짐이 떨어지거나 배기가스가 실내로 들어올 수 있음). 실제 차도 트렁크 열림을 계기판에 경고한다
- 트렁크를 빼야 하는 상황이 생긴다면? → 예를 들어 **긴 짐을 싣고 트렁크를 연 채 천천히 이동**하는 걸 허용하거나, "승객이 내리는 문"만 따로 봐야 하는 기능(예: 하차 시 뒤차 경고)이 생기면 트렁크를 빼야 한다. 그때는 함수를 바꾸지 않고 **새 함수를 추가**한다

```c
bool door_any_passenger_open(const DoorStatus *d);   /* 문 4개만 */
bool door_any_open(const DoorStatus *d);             /* 문 4개 + 트렁크 (기존 그대로) */
```

  - 기존 함수를 고치면 그걸 쓰던 다른 코드(M4 경고 로직)의 동작이 몰래 바뀐다 → 이름으로 의미를 나눠 두는 게 안전하다

## 5. 테스트 (이슈 완료 조건)

| 완료 조건 | 테스트 |
| --- | --- |
| 모든 도어 닫힘 | `test_door_all_closed` (`0x00` → 전부 false, `door_any_open` false) |
| 하나만 열림 (5가지) | `test_door_one_open_each` (`0x01`, `0x02`, `0x04`, `0x08`, `0x10` → 해당 문만 true) |
| 전부 열림 | `test_door_all_open` (`0x1F` → 전부 true) |
| (추가) 정의서 예시 | `test_door_spec_example` (`0x11` → FL + 트렁크) |
| (추가) 예약 비트 무시 | `test_door_ignores_reserved_bits` (`0xE0` + 나머지 `0xFF` → 전부 닫힘) |
| (추가) 해석 불가 | `test_door_cannot_decode` (DLC 0, 다른 ID, NULL → false) |
| (추가) 문자열 | `test_door_format` (`"FL=1 FR=0 RL=0 RR=0 TRUNK=1"`) |

- 결과 → `[test_signals]` **80 passed**, 0 failed (차속 24 + 도어 56)
- `test_door_one_open_each`는 테이블 기반 테스트다. 테스트 5개를 따로 쓰지 않고 표로 만든 이유 →
  - **"입력 → 기대값" 목록만 한눈에 보인다.** 검사 코드는 한 번만 쓰고, 경우는 표에 한 줄씩 추가하면 된다
  - 똑같은 검사 코드를 5번 복사하면 하나만 실수로 다르게 고쳐도 모른다
  - 새 경우가 생기면(예: 선루프 bit 5) **표에 한 줄**만 추가한다
  - JUnit의 `@ParameterizedTest` + `@CsvSource`와 같은 방식. M4-7 FSM 전이 표 테스트에서 본격적으로 쓴다
- `test_door_ignores_reserved_bits`에서 `0xE0`을 쓴 이유 → `0xE0` = `1110 0000`으로 **예약 비트(5~7)만 1이고 도어 비트(0~4)는 0**이다. 나머지 바이트도 `0xFF`로 채웠다. 디코더가 예약 비트를 실수로 읽으면 어떤 문이 "열림"으로 나온다 → 전부 닫힘이 나와야 **정확히 자기 비트만** 읽는다는 증거 (M1-3: 예약 비트는 받는 쪽이 무시)
