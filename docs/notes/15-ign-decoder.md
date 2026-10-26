# 15. 시동 & 기어 디코더 (M2-5)

> 코드: `include/signals.h`, `src/signals.c`, `tests/test_signals.c`
> 실행: `make test`

## 1. C의 `enum`

```c
typedef enum {
    GEAR_P = 0,
    GEAR_R = 1,
    GEAR_N = 2,
    GEAR_D = 3,
    GEAR_INVALID = 4
} Gear;
```

- `enum`이란? → **정수 값에 이름표를 붙인 것.** `GEAR_D`는 컴파일되면 그냥 정수 `3`이다. 관련된 상수 여러 개를 한 묶음(타입)으로 정리한다
- Java의 `enum`과 다른 점 →

| | Java `enum` | C `enum` |
| --- | --- | --- |
| 정체 | **객체** (클래스) | **그냥 정수** (보통 `int` 크기) |
| 정의 안 된 값 넣기 | 불가능 (`Gear.values()[9]` → 예외) | **가능** (`(Gear)9` → 에러 없음) |
| 메서드/필드 | 가질 수 있음 (`gear.name()`, `gear.isDriving()`) | 없음 → `gear_name()` 같은 함수를 따로 만든다 |
| 이름 → 문자열 | `GEAR_D.name()` 자동 | 자동 없음 → `switch`로 직접 |
| 정수와 섞기 | 안 됨 | 됨 (`int x = GEAR_D + 1;`도 컴파일됨) |

- `gear_name((Gear)9)`가 컴파일되고 "UNKNOWN"이 나오는 이유 (`test_enum_names`) → C의 enum은 정수라서 `(Gear)9`처럼 **아무 정수나 강제로 넣을 수 있다.** 9는 어떤 `case`에도 해당하지 않아서 `switch`의 `default`로 가고 "UNKNOWN"을 돌려준다. 이 테스트는 "enum에 이상한 값이 들어와도 함수가 터지지 않는다"를 확인한다
- 숫자 `3` 대신 `GEAR_D`라고 쓰는 이유 → **뜻이 보인다** (`if (gear == 3)`보다 `if (gear == GEAR_D)`). 정의서의 값이 바뀌어도 **enum 정의 한 곳만** 고치면 된다 (매직 넘버 금지, M1-2). 그리고 `switch`에서 case를 빠뜨리면 컴파일러가 `-Wswitch-enum`으로 알려줄 수 있다 (숫자로 쓰면 못 알려줌)

## 2. 한 메시지에 신호 두 개

| 신호 | start bit | 길이 | 위치 | 정의된 값 |
| --- | --- | --- | --- | --- |
| IgnState | 0 | 2 | data[0]의 비트 0~1 | 0 = OFF, 1 = ACC, 2 = ON, 3 = START (2비트로 표현 가능한 0~3이 **전부** 정의됨) |
| Gear | 8 | 3 | data[1]의 비트 0~2 | 0 = P, 1 = R, 2 = N, 3 = D (3비트의 0~7 중 **0~3만** 정의, 4~7은 무효) |

- 최소 길이가 2바이트인 이유 → Gear가 **data[1]**에 있기 때문이다 (start bit 8 ÷ 8 = 1번 바이트). DLC가 1이면 IgnState는 읽을 수 있어도 기어를 읽을 수 없다 → 반쪽짜리로 해석하지 않고 프레임 전체를 거부한다 (`test_ign_cannot_decode`)
- M1-3 연습 문제 D, E와의 관계 → 손으로 풀었던 문제를 그대로 테스트로 만들었다
  - 문제 D `300#0201...` → IgnState 2(ON), Gear 1(R): 손으로 `& 0x03`, `& 0x07` 했던 걸 `can_get_bits(data, 0, 2)`, `can_get_bits(data, 8, 3)`이 한다
  - 문제 E `300#0307...` → Gear 7 = 무효 (함정): `test_ign_exercise_e`가 `GEAR_INVALID`로 나오는지 확인한다

## 3. 범위 밖 값 처리 — Gear 4~7

```c
static Gear gear_from_raw(uint32_t raw) {
    switch (raw) {
        case 0u: return GEAR_P;
        ...
        default: return GEAR_INVALID;
    }
}
```

- 그냥 `(Gear)raw`로 넣으면 무슨 일이? → 에러 없이 **정의 안 된 값 5, 6, 7이 `Gear` 변수에 그대로** 들어간다 (C enum은 정수). 4는 우연히 `GEAR_INVALID`(= 4)와 같아서 맞는 것처럼 보이지만, 5~7은 어떤 enum 이름에도 해당하지 않는다. 그 뒤 `if (gear == GEAR_D)` 같은 비교는 전부 거짓이 되고, 코드는 "이상한 기어"라는 걸 모른 채 계속 진행한다
- `GEAR_INVALID`로 바꾸는 게 왜 안전한가? (M4에서 기어를 어떻게 쓸지 생각해 보기) → 무효 상황이 **이름 하나로 모인다.** M4에서는 `if (gear == GEAR_INVALID)` 한 줄로 "기어를 모르면 안전한 쪽으로"를 처리할 수 있다. 예를 들어 "P단이면 정차 중 → 잠금 해제" 규칙이 있을 때, 무효 기어를 P로 착각할 일이 없다. 5, 6, 7을 하나하나 검사할 필요도 없다
- IgnState는 `gear_from_raw()` 같은 변환 없이 `(IgnState)ign_raw`로 바로 넣었다. 괜찮은 이유 → IgnState는 **2비트**라서 꺼낸 값이 **0~3밖에 나올 수 없고**, 0~3이 **전부** 정의돼 있다 (OFF/ACC/ON/START). 범위 밖 값이 생길 수가 없으니 변환이 필요 없다. 반면 Gear는 3비트(0~7) 중 0~3만 정의돼 있어서 변환이 필요하다 (2번 표의 "정의된 값")
- 기어가 무효여도 `decode_ign_status()`가 `true`를 반환하는 이유 (M2-3 차속 무효값과 비교) → M2-3 차속과 같은 원칙: **프레임은 정상적으로 해석했고, 그 안의 신호 값이 "모름"인 것**이다. 프레임이 깨진 것(`false` → 버림)과 다르다. 게다가 같은 프레임의 IgnState는 정상 값이라 **버리면 아깝다** → true를 반환하고 gear만 `GEAR_INVALID`로 표시한다

| | 프레임 해석 불가 | 신호 값이 무효 |
| --- | --- | --- |
| 차속 (M2-3) | `false` | `true` + `valid = false` |
| 기어 (M2-5) | `false` | `true` + `gear = GEAR_INVALID` |

## 4. `switch`와 `default`

- `ign_state_name()`에 `default`가 있는 이유 (모든 case를 다 썼는데도) → C enum은 정수라 **정의 안 된 값(`(IgnState)9`)이 들어올 수 있다.** 그때 `default`가 없으면 어떤 `return`에도 안 걸리고 함수 끝으로 빠져나가 **쓰레기 값을 반환**한다(UB). `default`로 "UNKNOWN"을 돌려줘서 어떤 값이 와도 안전하게 끝낸다 (방어적 프로그래밍, MISRA-C도 모든 switch에 default를 요구)
- `-Wswitch-enum` 경고 옵션이 잡아주는 것 → enum으로 `switch` 할 때 **case를 하나라도 빠뜨리면** 경고한다. 예: 나중에 `GEAR_L`(저단)을 enum에 추가하고 `gear_name()`에 case를 안 넣으면 컴파일할 때 바로 알려준다. `default`가 있어도 경고해 줘서, "빠뜨림"과 "범위 밖 값 대비"를 둘 다 챙길 수 있다

```c
switch (g) {
    case GEAR_P: ...
    case GEAR_R: ...
    /* GEAR_N 빠뜨림 → warning: enumeration value 'GEAR_N' not explicitly handled in switch */
    ...
}
```

## 5. 테스트 (이슈 완료 조건)

| 완료 조건 | 테스트 |
| --- | --- |
| 정상 값 전부 | `test_ign_all_states` (IgnState 0~3 → OFF/ACC/ON/START), `test_gear_all_values`의 0~3 (P/R/N/D) |
| 범위 밖 값 | `test_gear_all_values`의 4~7 → 전부 `GEAR_INVALID` (프레임 해석은 true) |
| (추가) 정의서 예시 | `test_ign_spec_example` (`02 03` → ON, D) |
| (추가) 연습 문제 E | `test_ign_exercise_e` (`03 07` → START, INVALID) |
| (추가) 예약 비트 무시 | `test_ign_ignores_reserved_bits` (`FE F9` → ON, R) |
| (추가) 해석 불가 | `test_ign_cannot_decode` (DLC 1, 다른 ID, NULL → false) |
| (추가) 이름 변환 | `test_enum_names` (정상 이름 + `(Gear)9`, `(IgnState)9` → "UNKNOWN") |
| (추가) 문자열 | `test_ign_format` (`"IGN=ON GEAR=D"`) |

- 결과 → `[test_signals]` **125 passed**, 0 failed (차속 24 + 도어 56 + 시동/기어 45)
