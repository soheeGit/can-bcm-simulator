# 08. CanFrame 구조체 (M1-2)

> 코드: `include/can_frame.h`, `src/can_frame.c`, `tests/test_can_frame.c`
> 실행: `make && ./build/bcm_sim`, `make test`

## 1. 실행 결과

```
can-bcm-simulator v0.0.1
샘플 프레임: 0x100 [8] 10 27 00 00 00 00 00 00
유효한 프레임? yes
```

```
[test_can_frame]
  - test_valid_dlc_0
  - test_valid_dlc_8
  - test_invalid_dlc_9
  - test_invalid_dlc_15
  - test_valid_id_0x000
  - test_valid_id_0x7ff
  - test_invalid_id_0x800
  - test_invalid_null
  - test_format_full
  - test_format_short_dlc
  - test_format_dlc_0
  - test_format_invalid_dlc_does_not_overflow
  - test_format_small_buffer
  13 passed, 0 failed
```

## 2. 구조체 설계

```c
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[CAN_FRAME_MAX_DLC];
} CanFrame;
```

- `id`가 11비트인데 `uint32_t`인 이유 → `uint16_t`(16비트)로도 11비트는 담기지만, **Extended ID(29비트)**도 담을 수 있게 여유를 뒀다. Linux `struct can_frame`의 `can_id`도 32비트라서 M5에서 변환이 쉽다. (Linux는 남는 윗비트 3개에 EFF/RTR/ERR 표시를 넣는다 → M1-1 7번)
- `dlc`가 `uint8_t`인 이유 → 값이 0 ~ 8(CAN 프레임에서는 4비트)이라 1바이트면 충분하다. 음수가 없으니 unsigned
- `data`가 `uint8_t[8]`인 이유 → CAN 데이터는 **바이트 단위, 최대 8바이트**. `uint8_t` = 정확히 1바이트, 음수 없음 → 비트 연산 하기 좋다 (M0-5 "비트 연산은 항상 unsigned"). `char`는 플랫폼에 따라 signed일 수 있어서 쓰지 않는다
- `sizeof(CanFrame)`은 몇일까? (M0-3 실험 5 패딩 복습) → **16.** 멤버 합은 4 + 1 + 8 = 13이지만, `uint32_t id` 때문에 구조체 전체 크기를 4의 배수로 맞추려고 **끝에 3바이트 패딩**이 붙는다. M0-3 실험 5의 `CanFrameLike`와 똑같은 모양

```
바이트: 0 1 2 3 | 4   | 5 ~ 12  | 13 14 15
        id      | dlc | data[8] | 패딩
```

- `typedef struct { ... } CanFrame;`의 `typedef`는 무슨 일을 하나? → 타입에 **별명**을 붙인다. typedef가 없으면 매번 `struct CanFrame f;`처럼 `struct`를 붙여 써야 하는데, typedef 덕분에 `CanFrame f;`로 쓸 수 있다. Java에서 클래스 이름으로 바로 쓰는 것과 비슷하게 만들어 준다

## 3. 헤더 가드

```c
#ifndef CAN_FRAME_H
#define CAN_FRAME_H
...
#endif
```

- 이게 없으면 무슨 일이 생기나? (같은 헤더를 두 번 include하면?) → `#include`는 **복사 붙여넣기**라서 `CanFrame` 정의가 두 번 들어가 컴파일 에러가 난다. 다른 헤더를 거쳐서 두 번 들어오는 경우가 흔하다 (예: `main.c`가 `can_frame.h`와 `can_signal.h`를 include하는데, `can_signal.h` 안에서도 `can_frame.h`를 include)

```
(헤더 가드를 지우고 두 번 include 했을 때)
error: typedef redefinition with different types ('struct CanFrame' vs 'struct CanFrame')
note: './can_frame.h' included multiple times
```

- 동작 원리 (출석부 비유)
  - 처음: `CAN_FRAME_H`가 정의 안 됨(`#ifndef` 참) → `#define CAN_FRAME_H`로 "출석 표시" → 내용 적용
  - 두 번째: 이미 정의됨 → `#endif`까지 통째로 건너뜀
- 규칙 → 이름은 파일 이름을 대문자로(`can_frame.h` → `CAN_FRAME_H`). **모든 헤더에 넣는다.** `#pragma once` 한 줄로도 되지만 C 표준이 아니라서 자동차 업계는 `#ifndef` 방식을 많이 쓴다

## 4. 상수 (매직 넘버 금지)

| 상수 | 값 | 뜻 |
| --- | --- | --- |
| `CAN_FRAME_MAX_DLC` | `8u` | 일반 CAN 데이터 최대 길이(바이트). `data` 배열 크기와 DLC 검사에 같이 쓴다 |
| `CAN_FRAME_STD_ID_MAX` | `0x7FFu` | Standard ID(11비트) 최댓값. ID 검사에 쓴다 |
| `CAN_FRAME_STR_SIZE` | `48u` | `can_frame_format()`에 넘길 문자열 버퍼 크기. `"0x7FF [8] FF FF FF FF FF FF FF FF"`(34자) + `'\0'`보다 넉넉하게 |

- 그냥 `8`, `0x7FF`라고 쓰지 않는 이유 → **매직 넘버** 방지. `8`만 보면 "이게 DLC 최대값인지, 비트 수인지" 알 수 없다. 이름이 뜻을 알려준다. 그리고 값을 바꿔야 할 때(예: CAN FD로 64) **한 곳만** 고치면 된다. Java의 `static final int`와 같은 역할
- 이름 앞에 `CAN_FRAME_`를 붙인 이유 (힌트: `<linux/can.h>`) → Linux 헤더에 `CAN_MAX_DLC`, `CAN_SFF_MASK` 같은 이름이 **이미 있다.** M5에서 SocketCAN을 쓰려고 `<linux/can.h>`를 같이 include하면 같은 이름이 두 번 정의돼 충돌한다. C에는 namespace가 없으니 **모듈 이름 접두사**로 피한다 (M0-7 `bit_` 접두사와 같은 이유)

## 5. `can_frame_is_valid()` 경계값 테스트

| 테스트 | 입력 | 기대 | 이유 |
| --- | --- | --- | --- |
| `test_valid_dlc_0` | DLC 0 | ✅ 유효 | 데이터 없는 프레임도 정상이다 (가장 작은 값) |
| `test_valid_dlc_8` | DLC 8 | ✅ 유효 | 최대 길이 (유효 범위의 끝) |
| `test_invalid_dlc_9` | DLC 9 | ❌ 거부 | 최대보다 1 큼 (무효 범위의 시작) |
| `test_invalid_dlc_15` | DLC 15 | ❌ 거부 | DLC 4비트로 적을 수 있는 최댓값 |
| `test_valid_id_0x000` | ID 0x000 | ✅ 유효 | 가장 작은 ID |
| `test_valid_id_0x7ff` | ID 0x7FF | ✅ 유효 | 11비트 최댓값 (유효 범위의 끝) |
| `test_invalid_id_0x800` | ID 0x800 | ❌ 거부 | 12번째 비트 필요 (무효 범위의 시작) |
| `test_invalid_null` | NULL | ❌ 거부 | 포인터가 비어 있으면 읽지 않고 false (Segfault 방지, M0-3) |

- 경계값(0, 8, 9 / 0x7FF, 0x800)을 고른 이유 → 버그는 **경계에서** 잘 생긴다 (off-by-one: `<`와 `<=` 한 글자 차이, M0-6 실험 2). "유효 범위의 끝"과 "무효 범위의 시작"을 둘 다 테스트하면, 검사 조건을 `>`로 썼는지 `>=`로 썼는지 실수가 바로 드러난다. 예를 들어 `dlc >= 8`로 잘못 쓰면 `test_valid_dlc_8`이 실패한다
- DLC는 4비트(0~15)인데 왜 0~8까지만 유효한가? → 데이터 길이 0~8은 **9가지**라서 3비트(0~7)로는 8을 못 담는다 → 4비트가 필요하다. 4비트라 9~15도 적을 수는 있지만 일반 CAN에서는 의미가 없다(규격상 8바이트로 취급). 남는 9~15는 **CAN FD가 12~64바이트로 재활용**했다.

```
7 = 0111   ← 3비트 최댓값
8 = 1000   ← 4번째 비트가 필요
```

  - 우리 코드는 9 이상을 **거부**한다 → 정상 제어기는 안 보내는 값이라, 오면 고장/잘못된 로그일 가능성이 크다 (방어적 프로그래밍). 단 `can_frame_format()`은 로그 출력용으로 8바이트까지만 읽는다

## 6. `can_frame_format()` — 안전한 문자열 만들기

- `snprintf`를 쓴 이유 (M0-5에서 배운 것) → 버퍼 크기를 넘겨서 **절대 넘치지 않게** 하려고. `sprintf`는 크기 제한이 없어서 버퍼 오버플로우가 날 수 있다. 바이트를 하나씩 이어 붙이면서 `buf + written`(이미 쓴 다음 위치), `size - written`(남은 공간)을 넘긴다
- DLC가 15인 잘못된 프레임에서 data[8] 밖을 읽지 않게 막은 코드 →

```c
uint8_t count = (f->dlc > CAN_FRAME_MAX_DLC) ? (uint8_t)CAN_FRAME_MAX_DLC : f->dlc;
for (uint8_t i = 0; i < count; i++) { ... f->data[i] ... }
```

  - DLC를 그대로 믿고 15번 반복하면 `data[8] ~ data[14]`(남의 메모리)를 읽는다 (M0-6 실험 2의 범위 밖 접근). 그래서 **최대 8번**만 돌게 제한한다. 결과는 `0x100 [15] 01 02 ... 08` (DLC 값은 그대로 보여주고 데이터는 8개만)
- `test_format_small_buffer`가 확인하는 것 → 버퍼가 6바이트밖에 없어도 **넘치지 않고 잘리는지.** `"0x100"`(5자) + `'\0'` = 6바이트에서 멈춰야 한다. 이 테스트를 Sanitizer 빌드로 돌려도 에러가 없었다
- `can_frame_print()`를 따로 두지 않고 `can_frame_format()`부터 만든 이유 (테스트 관점) → 화면에 바로 찍는 함수는 결과를 **테스트로 확인하기 어렵다** (화면 출력을 비교해야 함). 문자열로 만드는 함수는 `strcmp`로 기대값과 바로 비교할 수 있다. 그래서 핵심 로직은 `format`에 두고, `print`는 `format` 결과를 찍기만 하는 얇은 함수로 만들었다. (Java에서 `toString()`을 테스트하고 `System.out.println`은 테스트하지 않는 것과 같다)

## 7. 지정 초기화 (designated initializer)

```c
CanFrame f = { .id = 0x100, .dlc = 8, .data = {0x10, 0x27} };
```

- `.id = ...` 문법의 뜻 → **필드 이름을 붙여서** 값을 넣는 초기화 (C99부터). 순서대로 쓰는 `{ 0x100, 8, {0x10, 0x27} }`보다:
  - 읽기 쉽다 (어느 값이 어느 필드인지 보임)
  - 구조체 필드 순서가 바뀌어도 안전하다 (순서 방식은 엉뚱한 필드에 값이 들어가도 에러가 안 남)
  - 순서를 바꿔 써도 된다: `{ .dlc = 8, .id = 0x100 }`
- 적지 않은 `data[2] ~ data[7]`의 값은? → **0.** `= { ... }`로 초기화하면 적지 않은 필드/배열 칸은 전부 0이 된다 → 그래서 출력이 `10 27 00 00 00 00 00 00`
  - ⚠️ 그냥 `CanFrame f;`만 쓰면 초기화가 안 돼서 **쓰레기 값** (M0-6 실험 4) → 구조체는 항상 `= { ... }` 또는 `= {0}`으로 초기화하는 습관
- Java로 치면? → 빌더 패턴 `CanFrame.builder().id(0x100).dlc(8).build()` (이름 붙여 넣고, 안 넣은 건 기본값). Kotlin의 이름 붙인 인자 `CanFrame(id = 0x100, dlc = 8)`와 같다

## 8. Linux `struct can_frame`과 비교

| | 우리 `CanFrame` | Linux `struct can_frame` |
| --- | --- | --- |
| ID | `uint32_t id` (11비트 값만) | `canid_t can_id` (32비트, ID + 윗비트 플래그) |
| 길이 | `uint8_t dlc` | `__u8 len` (예전 이름 `can_dlc`) |
| 데이터 | `uint8_t data[8]` | `__u8 data[8]` (8바이트 정렬) |
| 종류 표시(RTR/ERR/EFF) | 없음 (Data Frame + Standard ID만 다룸) | `can_id`의 31/30/29번 비트 (`CAN_EFF_FLAG`, `CAN_RTR_FLAG`, `CAN_ERR_FLAG`) |
| 그 외 | 없음 | 예약 바이트(`__pad`, `__res0`), DLC 9~15 원래 값 보관용 `len8_dlc` |
| 크기 | 16바이트 (13 + 패딩 3) | 16바이트 |

- 구성이 거의 같아서 M5-4에서 변환 함수 하나면 된다:

```c
/* M5-4 미리보기 */
if (lf.can_id & (CAN_RTR_FLAG | CAN_ERR_FLAG)) { /* Remote/Error 무시 */ }
f.id  = lf.can_id & CAN_SFF_MASK;   /* 플래그를 떼고 11비트 ID만 */
f.dlc = lf.len;
memcpy(f.data, lf.data, 8);
```

- 왜 Linux 구조체를 그대로 안 쓰나? → ① `<linux/can.h>`는 **맥에서 없다** → M0~M4는 맥에서도 빌드돼야 한다 ② 우리 코드는 Linux에 묶이지 않고, M3의 로그 파일 입력도 같은 `CanFrame`으로 받는다 (입력 소스 추상화, M5-5)
