# 06. 단위 테스트 환경 (M0-7)

> 이번엔 playground가 아니라 **진짜 프로젝트 코드**에 들어간다
>
> - `tests/test_framework.h` : 직접 만든 테스트 매크로 (`ASSERT_EQ`, `ASSERT_TRUE`, ...)
> - `include/bit_utils.h`, `src/bit_utils.c` : M0-5 비트 함수를 프로젝트 모듈로 옮긴 것
> - `tests/test_bit_utils.c` : 위 모듈의 테스트
> - 실행: `make test`

## 실험 1: `make test` 실행

```
mkdir -p build/tests
gcc -std=c11 -Wall -Wextra -Werror -g -Iinclude -Itests tests/test_bit_utils.c build/bit_utils.o -o build/tests/test_bit_utils
[test_bit_utils]
  - test_bit_is_set
  - test_bit_set
  - test_bit_clear
  - test_bit_toggle
  - test_bit_count
  - test_door_status_scenario
  23 passed, 0 failed
✅ 모든 테스트 통과
```

- 몇 개 통과? → 23개 (테스트 함수 6개, 그 안의 ASSERT 23개)
- `echo $?` 결과 → **0** (모든 테스트 통과 → 성공)
- `make test`가 실행한 명령 순서 (빌드 → 링크 → 실행) →
  1. `src/bit_utils.c` → `build/bit_utils.o` 컴파일 — **이번 출력에는 없다.** 이미 만들어져 있고 소스가 안 바뀌어서 Make가 건너뛰었다 (M0-2 증분 빌드). `make clean` 후 다시 하면 보인다.
  2. `mkdir -p build/tests` — 테스트 실행 파일을 넣을 폴더
  3. `gcc ... tests/test_bit_utils.c build/bit_utils.o -o build/tests/test_bit_utils` — 테스트 코드 컴파일 + `bit_utils.o`와 **링크**를 한 번에 (`main.o`는 빠짐)
  4. `./build/tests/test_bit_utils` 실행 — `[test_bit_utils]`부터가 테스트 프로그램의 출력
  5. 종료 코드를 보고 `✅ 모든 테스트 통과` 출력

## 실험 2: 테스트를 일부러 실패시켜 보기

`tests/test_bit_utils.c`에서 아무 기대값 하나를 틀리게 바꾼다 (예: `ASSERT_EQ(4, bit_count(0xB4u));` → `ASSERT_EQ(5, ...)`)

```
mkdir -p build/tests
gcc -std=c11 -Wall -Wextra -Werror -g -Iinclude -Itests tests/test_bit_utils.c build/bit_utils.o -o build/tests/test_bit_utils
[test_bit_utils]
  - test_bit_is_set
    FAIL tests/test_bit_utils.c:5: bit_is_set(0xB4u, 20) 가 참이어야 함
  - test_bit_set
  - test_bit_clear
  - test_bit_toggle
  - test_bit_count
  - test_door_status_scenario
  22 passed, 1 failed
❌ 테스트 실패
make: *** [test] Error 1
```

- 바꾼 곳 → 5번째 줄 `bit_is_set(0xB4u, 2)` → `bit_is_set(0xB4u, 20)`. 0xB4는 하위 8비트에만 1이 있어서 20번 비트는 0 → false → "참이어야 함"이 실패
- FAIL 메시지가 알려주는 것 3가지 →
  1. **어느 테스트 함수에서** — `- test_bit_is_set` 바로 아래에 FAIL이 찍힌다
  2. **어느 파일 몇 번째 줄** — `tests/test_bit_utils.c:5`
  3. **어떤 코드가 무엇을 기대했는지** — `bit_is_set(0xB4u, 20) 가 참이어야 함` (`ASSERT_EQ`라면 기대값과 실제값도 출력)
  - 하나가 실패해도 **나머지 테스트는 끝까지 실행**된다 (22 passed, 1 failed) → 한 번에 실패한 것들을 다 볼 수 있다
- `echo $?` 결과 → **2**. 테스트 프로그램은 1을 반환했고(`TEST_SUMMARY()`), 이걸 본 Makefile이 `exit 1`로 실패 → **make 자체가 실패하면 make는 2를 반환**한다 (`make: *** [test] Error 1`). 0이 아니면 실패라는 것만 중요하다.
- M0-5 실험 4(`make`는 성공했던 것)와 다른 점 → M0-5에서는 `make`가 **빌드만** 하고 테스트는 내가 따로 실행해야 했다. 테스트가 실패해도 make는 성공(0). 이제는 `make test`가 **빌드 + 실행 + 결과 판정**까지 해서, 테스트 실패 = make 실패. → CI(M6-1)가 "make test가 실패하면 PR 머지 금지"를 걸 수 있다.
- 끝나면 원래대로 되돌리기 ✅ (20 → 2 복구 확인)

## 실험 3: 범위 밖 비트 번호 (`n >= 32`)

- `bit_set(0xB4u, 32)`의 결과와 이유 → **0xB4 그대로.** 32비트 값에는 0~31번 비트만 있어서 32번 비트는 없다. `bit_set` 안의 `if (n >= 32u) { return x; }` 검사가 아무것도 안 하고 원래 값을 돌려준다. (테스트: `ASSERT_EQ(0xB4u, bit_set(0xB4u, 32));`)
- 이 검사를 넣은 이유 (M0-6 실험 3과 연결) → `bit_set`은 ① `1u << n`으로 "n번 칸에만 1이 있는 마스크"를 만들고 ② `x | 마스크`로 그 칸을 켠다. 그런데 n = 32면 ①에서 "1을 **32번 칸**으로 옮겨라"가 되는데, 32번 칸은 없다(칸은 0~31번). 맥은 이때 1을 **0번 칸**에 놓아버린다(M0-6 실험 3).

```
① 1u << 32 = 0000 ... 0000 0001   ← 32번 칸이 없어서 0번 칸에 놓임
② 0xB4 | ① = 1011 0100
           | 0000 0001
           = 1011 0101 (0xB5)     ← "32번 칸 켜줘"였는데 0번 칸이 켜짐!
```

  - 도어 상태라면 "존재하지 않는 32번 문을 열어라" → **운전석(0번) 문이 열린 것으로 바뀐다.** 에러도 없이 조용히.
  - 그래서 함수 입구에서 잘못된 입력을 걸러낸다 (**방어적 프로그래밍**). Java라면 `IllegalArgumentException`을 던질 상황이지만 C에는 예외가 없어서, "아무것도 안 하기 / false 반환"으로 처리하고 헤더 주석에 규칙을 적어둔다.
  - M2-1 `can_get_bits(data, start_bit, length)`도 같은 검사가 필요하다 (`length > 32`, `start_bit + length > 64`)

## 구조 이해

### 매크로 vs 함수

- `ASSERT_EQ`를 함수가 아니라 매크로(`#define`)로 만든 이유 (힌트: `__FILE__`, `__LINE__`, `#actual`) → FAIL 메시지에 **파일 이름, 줄 번호, 실패한 코드**를 넣기 위해서다. 매크로는 전처리기(M0-2)가 코드를 **쓴 자리에 그대로 복사해 붙여 넣기** 때문에:

| 특수 기호 | 뜻 | 예시 |
| --- | --- | --- |
| `__FILE__` | 이 코드가 있는 파일 이름 | `tests/test_bit_utils.c` |
| `__LINE__` | 이 코드가 있는 줄 번호 | `12` |
| `#actual` | 넘겨받은 코드를 **글자 그대로** 문자열로 | `"bit_set(0xB4u, 0)"` |

  - 매크로라서 `__LINE__`이 **`ASSERT_EQ`를 쓴 그 줄**의 번호가 된다. 함수였다면 항상 함수 정의 안쪽 줄 번호가 찍혀서 어디서 실패했는지 알 수 없다.
  - `#actual`(코드를 글자로 바꾸기)은 함수로는 아예 불가능하다. 함수는 이미 계산된 **값**만 받기 때문.
  - 단점: 매크로는 타입 검사가 없고, 에러 메시지가 복잡해진다 → 이런 특수한 경우에만 쓰고, 보통은 함수를 쓴다.
- `do { ... } while (0)`로 감싼 이유 → 매크로 안의 여러 줄을 **한 문장처럼** 묶기 위해서다. 그냥 `{ ... }`로만 감싸면 `if`/`else`와 같이 쓸 때 깨진다:

```c
if (cond)
    ASSERT_EQ(1, x);   // { ... };  ← 끝의 ; 때문에 if 문이 여기서 끝나버림
else                   // ← 짝 잃은 else → 컴파일 에러
    ...
```

  - `do { ... } while (0)`은 딱 한 번 실행되고, 뒤에 `;`를 붙여야 완성되는 **한 문장**이라 어디에 써도 안전하다. C 매크로의 관용구.

### 테스트 빌드는 어떻게 되나

```
tests/test_bit_utils.c ─┐
                        ├─ 링크 → build/tests/test_bit_utils
build/bit_utils.o ──────┘     (src/main.c는 빠짐)
```

- 테스트 빌드에서 `main.o`를 빼는 이유 → C 프로그램에는 `main` 함수가 **딱 하나**만 있어야 한다. `src/main.c`에도 `main`이 있고 `tests/test_bit_utils.c`에도 `main`이 있어서, 둘 다 링크하면 링커가 "`main`이 두 개"라며 에러를 낸다 (`duplicate symbol '_main'`). M0-2 실험 A는 정의가 **없어서**, 이건 정의가 **두 개라서** 생기는 링크 에러다.
  - 그래서 Makefile에서 `LIB_OBJS := $(filter-out $(BUILD)/main.o,$(OBJS))`로 `main.o`만 뺀 목록을 만들어 테스트에 링크한다.
  - 프로그램 = `main.c`(진입점) + 기능 모듈들. 테스트 = `test_xxx.c`(테스트용 진입점) + **같은** 기능 모듈들. 진입점만 바꿔 끼우는 구조.

```
실제 프로그램:  src/main.c            + build/bit_utils.o → build/bcm_sim
테스트:        tests/test_bit_utils.c + build/bit_utils.o → build/tests/test_bit_utils
               └─ 진입점(main)만 다름 ─┘  └─ 기능은 같은 .o ─┘
```

- 앞으로 `src/can_signal.c`를 만들고 `tests/test_can_signal.c`를 추가하면 Makefile을 고쳐야 하나? → **안 고쳐도 된다.** Makefile이 와일드카드로 자동으로 찾는다:
  - `SRCS := $(wildcard src/*.c)` → `can_signal.c`가 자동으로 `LIB_OBJS`에 들어감
  - `TEST_SRCS := $(wildcard tests/test_*.c)` → `test_can_signal.c`가 자동으로 테스트 목록에 들어감
  - 규칙: 테스트 파일 이름은 반드시 `test_`로 시작해야 한다 (`test_framework.h`는 `.h`라서 제외됨)
  - 테스트 실행 파일 하나에 **모든 기능 모듈**이 링크되므로, `test_can_signal`이 `bit_utils`의 함수를 써도 된다

### 함수 이름 앞에 `bit_`를 붙인 이유

- C에는 Java의 `package`나 `namespace`가 없다 → 프로그램 전체의 함수 이름이 **한 공간**에 모인다. 다른 파일(또는 라이브러리)에 같은 이름의 함수가 있으면 링크할 때 `duplicate symbol` 에러가 난다 (`main`이 두 개일 때와 같은 에러).
  - 예: 나중에 다른 모듈에서도 `set_bit`라는 흔한 이름을 쓰면 충돌 → 그래서 **모듈 이름을 접두사**로 붙여 이름을 구분한다
  - Java였다면 `BitUtils.set()`처럼 클래스/패키지가 이름을 구분해 준다. C는 이름 자체에 소속을 적는다:

| Java | C (이 프로젝트) |
| --- | --- |
| `BitUtils.set(x, n)` | `bit_set(x, n)` |
| `BitUtils.isSet(x, n)` | `bit_is_set(x, n)` |
| `CanFrame.isValid(f)` | `can_frame_is_valid(f)` (M1-2 예정) |
| `Bcm.step(...)` | `bcm_step(...)` (M4-2 예정) |

  - 규칙: **`모듈이름_동사`** 형태. 파일 이름(`bit_utils.c`)과 접두사(`bit_`)를 맞추면 함수가 어느 파일에 있는지 바로 알 수 있다.
  - 파일 안에서만 쓰는 함수는 `static`을 붙이면 다른 파일에서 안 보여서 충돌하지 않는다 (Java의 `private` 같은 역할). 테스트 파일의 `static void test_bit_set(void)`가 그 예.

## 비교: 직접 만든 프레임워크 vs Unity (선택)

| | 직접 만든 `test_framework.h` | Unity (ThrowTheSwitch) |
| --- | --- | --- |
| 설치 | 없음. 헤더 파일 하나(`tests/test_framework.h`, 약 80줄) | 소스 파일 몇 개(`unity.c`, `unity.h`)를 프로젝트에 복사해서 같이 빌드 |
| 기능 | `ASSERT_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`, `RUN_TEST`, 요약/종료 코드 | 위 기능 + 타입별 비교(`TEST_ASSERT_EQUAL_UINT8`, `_HEX16` 등), 실수 오차 비교, 배열/문자열 비교, 테스트마다 `setUp`/`tearDown`, 테스트 러너 자동 생성 스크립트 |
| 실무 사용 | 학습용. 원리(매크로, `__LINE__`, 종료 코드)를 이해하기 좋다 | 임베디드 C에서 널리 쓰임. PC와 MCU(보드 위)에서 모두 돌아가도록 작고 가볍게 만들어짐. 목(mock) 생성 도구 CMock과 같이 씀 |
| 이 프로젝트 | 지금은 이걸로 충분 | 테스트가 많아지면(M4쯤) 교체 검토. `ASSERT_EQ` → `TEST_ASSERT_EQUAL` 식으로 이름만 바꾸면 돼서 옮기기 쉽다 |

## Java(JUnit)와 비교

| JUnit | 이 프로젝트 |
| --- | --- |
| `@Test void testX()` | `static void test_x(void)` + `main`에 `RUN_TEST(test_x);` 추가 (C는 어노테이션이 없다) |
| `assertEquals(expected, actual)` | `ASSERT_EQ(expected, actual)` — 실패 시 파일:줄, 코드, 기대값/실제값 출력 |
| `assertTrue(cond)` | `ASSERT_TRUE(cond)` / `ASSERT_FALSE(cond)` |
| 테스트 러너가 자동으로 찾아 실행 | 자동으로 못 찾는다 (C에는 리플렉션이 없음). 테스트 파일마다 `main`을 두고 `RUN_TEST`로 **직접 목록 작성** → 추가를 깜빡하면 그 테스트는 실행 안 됨 주의 |
| 테스트 클래스 자동 탐색 (`*Test.java`) | Makefile이 `tests/test_*.c`를 와일드카드로 자동 탐색 (파일 단위로는 자동) |
| `./gradlew test` | `make test` |
| 실패 시 빌드 실패 | 테스트 프로그램이 종료 코드 1 반환 → Makefile이 `exit 1` → `make test` 실패 (종료 코드 2) |
