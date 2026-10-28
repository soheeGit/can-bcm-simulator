# 21. 결과 로그 출력 & 로그 레벨 (M3-5)

> 코드: `include/logger.h`, `src/logger.c`, `tests/test_logger.c`, `src/log_reader.c`, `src/main.c`
> 실행: `make` 후
> - `./build/bcm_sim data/scenarios/01_normal_drive.log` (기본 = info)
> - `./build/bcm_sim data/scenarios/01_normal_drive.log debug` (레벨은 debug / info / warn / error / off 중 **하나**)
>
> ⚠️ 사용법의 `[debug|info|...]`는 "이 중 하나를 골라 써도 된다"는 **표기법**이다. 그대로 치면 `|`가 셸의 파이프로 해석돼서 `command not found`가 난다

## 1. 실행 결과

```
[   0.000s] INFO  can-bcm-simulator v0.0.1 시작: data/scenarios/01_normal_drive.log (로그 레벨 INFO)
[   0.000s] INFO  시동  IGN=OFF GEAR=P
[   0.000s] INFO  도어  FL=0 FR=0 RL=0 RR=0 TRUNK=0
[   0.000s] INFO  차속  0.00 km/h
[   0.500s] INFO  시동  IGN=ON GEAR=P
[   1.000s] INFO  시동  IGN=ON GEAR=D
[   1.100s] INFO  차속  2.00 km/h
[   1.200s] INFO  차속  4.00 km/h
[   1.300s] INFO  차속  6.00 km/h
[   1.400s] INFO  차속  8.00 km/h
[   1.500s] INFO  차속  10.00 km/h
[   1.600s] INFO  차속  12.00 km/h
[   1.700s] INFO  차속  14.00 km/h
[   1.800s] INFO  차속  16.00 km/h
[   1.900s] INFO  차속  18.00 km/h
[   2.000s] INFO  차속  20.00 km/h
[   2.100s] INFO  차속  22.00 km/h
[   2.200s] INFO  차속  24.00 km/h
[   2.300s] INFO  차속  26.00 km/h
[   2.400s] INFO  차속  28.00 km/h
[   2.500s] INFO  차속  30.00 km/h
[   2.600s] INFO  차속  32.00 km/h
[   2.700s] INFO  차속  34.00 km/h
[   2.800s] INFO  차속  36.00 km/h
[   2.900s] INFO  차속  38.00 km/h
[   3.000s] INFO  차속  40.00 km/h
[   3.100s] INFO  차속  42.00 km/h
[   3.200s] INFO  차속  44.00 km/h
[   3.300s] INFO  차속  46.00 km/h
[   3.400s] INFO  차속  48.00 km/h
[   3.500s] INFO  차속  50.00 km/h
[   3.600s] INFO  차속  52.00 km/h
[   3.700s] INFO  차속  54.00 km/h
[   3.800s] INFO  차속  56.00 km/h
[   3.900s] INFO  차속  58.00 km/h
[   4.000s] INFO  차속  60.00 km/h
[   6.100s] INFO  차속  58.00 km/h
[   6.200s] INFO  차속  56.00 km/h
[   6.300s] INFO  차속  54.00 km/h
[   6.400s] INFO  차속  52.00 km/h
[   6.500s] INFO  차속  50.00 km/h
[   6.600s] INFO  차속  48.00 km/h
[   6.700s] INFO  차속  46.00 km/h
[   6.800s] INFO  차속  44.00 km/h
[   6.900s] INFO  차속  42.00 km/h
[   7.000s] INFO  차속  40.00 km/h
[   7.100s] INFO  차속  38.00 km/h
[   7.200s] INFO  차속  36.00 km/h
[   7.300s] INFO  차속  34.00 km/h
[   7.400s] INFO  차속  32.00 km/h
[   7.500s] INFO  차속  30.00 km/h
[   7.600s] INFO  차속  28.00 km/h
[   7.700s] INFO  차속  26.00 km/h
[   7.800s] INFO  차속  24.00 km/h
[   7.900s] INFO  차속  22.00 km/h
[   8.000s] INFO  차속  20.00 km/h
[   8.100s] INFO  차속  18.00 km/h
[   8.200s] INFO  차속  16.00 km/h
[   8.300s] INFO  차속  14.00 km/h
[   8.400s] INFO  차속  12.00 km/h
[   8.500s] INFO  차속  10.00 km/h
[   8.600s] INFO  차속  8.00 km/h
[   8.700s] INFO  차속  6.00 km/h
[   8.800s] INFO  차속  4.00 km/h
[   8.900s] INFO  차속  2.00 km/h
[   9.000s] INFO  차속  0.00 km/h
[   9.500s] INFO  시동  IGN=ON GEAR=P
[  10.000s] INFO  시동  IGN=OFF GEAR=P

결과
  read 318, ok 318, error 0  (빈 줄/주석 0)
  디스패처: ok=318 ignored=0 error=0

최종 차량 상태
  차속 : 0.00 km/h
  도어 : FL=0 FR=0 RL=0 RR=0 TRUNK=0
  시동 : IGN=OFF GEAR=P
```

- 318줄짜리 시나리오인데 INFO 로그가 훨씬 적게 나오는 이유 → INFO는 **값이 처음 들어왔거나 바뀌었을 때만** 찍는다. 프레임은 100ms마다 계속 오지만 대부분 같은 값이다 (예: 4.0~6.0초 60 km/h 정속 구간은 차속 프레임 20개가 다 같아서 **4.0초 한 줄만** 찍힘, 문은 내내 닫혀 있어서 0초 한 줄만 찍힘). 그래서 318줄 → INFO 약 70줄
  - 위 로그에서 **4.000s 다음이 바로 6.100s**인 게 그 증거
  - 로그를 읽으면 "언제 무슨 일이 일어났는지"만 남는다: 0.5초 시동 ON → 1.0초 D → 가속 → 4.0초 60 km/h → 6.1초부터 감속 → 9.0초 정차 → 9.5초 P → 10.0초 시동 OFF (README의 시간표와 같다)
- 같은 파일을 `warn`, `debug`로 실행하면 무엇이 달라지나? →
  - `warn`: 01은 정상 파일이라 **로그가 한 줄도 안 나오고** 결과 요약만 나온다 (시작 메시지도 INFO라서 안 나옴). 04처럼 문제가 있는 파일이면 잘못된 줄만 보인다 → "문제만 빨리 찾고 싶을 때"
  - `debug`: **모든 프레임**(318개)이 `(log_reader.c:48) RX 0x100 [8] ... → OK`처럼 찍히고, 그 사이사이에 INFO가 섞인다 → 약 390줄. "어떤 프레임이 들어왔는지 하나하나 확인하고 싶을 때"
  - 결과 요약(`결과`, `최종 차량 상태`)은 레벨과 상관없이 항상 나온다 (로그가 아니라 프로그램의 출력)

## 2. 로그 레벨

| 레벨 | 이 프로젝트에서 찍는 것 | 예시 |
| --- | --- | --- |
| DEBUG | **모든 프레임**과 디스패처 결과, 찍은 위치 `(파일:줄)` | `[0.000s] DEBUG (log_reader.c:48) RX 0x300 [8] 00 00 ... → OK` |
| INFO | 프로그램 시작, 신호 값이 **처음 들어오거나 바뀔 때** | `[4.000s] INFO  도어  FL=1 FR=0 RL=0 RR=0 TRUNK=0` |
| WARN | 문제가 있지만 **계속 진행할 수 있는** 것: 형식이 틀린 줄, 해석 못 한 프레임 | `[1.700s] WARN  줄 57: ERR_DATA: (1.710000) vcan0 100#10G7...` |
| ERROR | **더 진행할 수 없는** 문제: 파일을 못 엶, 읽기 오류 | `[0.000s] ERROR 파일을 열 수 없습니다: nope.log (No such file or directory)` |
| (OFF) | 아무것도 안 찍음 (테스트 중) | |

- 레벨을 나누는 이유 → **상황에 따라 보고 싶은 양이 다르기 때문.** 평소에는 무슨 일이 있었는지만(INFO), 문제를 쫓을 때는 하나하나 다(DEBUG), 오래 돌리는 운영 환경에서는 문제만(WARN) 본다. 레벨이 없으면 "다 찍어서 못 읽거나", "안 찍어서 아무것도 모르거나" 둘 중 하나가 된다. 그리고 레벨을 바꿀 때 **코드를 고치지 않고** 실행할 때 고를 수 있다 (`./bcm_sim 파일 debug`)
  - 레벨은 **높을수록 심각**하고, 설정한 레벨 **이상**만 찍힌다: `WARN`으로 두면 WARN과 ERROR만
- Java(SLF4J)와 비교 →

| SLF4J / Logback | 이 프로젝트 |
| --- | --- |
| `log.debug("RX {}", frame)` | `LOG_DEBUG("RX %s", frame_str)` |
| `log.info(...)`, `log.warn(...)`, `log.error(...)` | `LOG_INFO(...)`, `LOG_WARN(...)`, `LOG_ERROR(...)` |
| `logback.xml`의 `<root level="INFO">` | `log_set_level(LOG_LEVEL_INFO)` / 실행 인자 `info` |
| 패턴 `%d %-5level %logger - %msg` | `[시각] 레벨 메시지` (`log_write` 안에서) |
| Appender (콘솔, 파일) | `log_set_output(FILE *)` |
| `{}` 자리 표시자 | `printf` 형식 (`%s`, `%u`) |

  - 차이: SLF4J는 클래스마다 로거가 따로 있고(`LoggerFactory.getLogger(X.class)`) 패키지별로 레벨을 다르게 줄 수 있다. 이 프로젝트는 프로그램 전체에 로거가 **하나**다 (작은 프로그램이라 충분)
  - 시각: SLF4J는 실제 시각을 찍지만, 여기서는 **시뮬레이션 시각**(로그 파일 속 시각)을 찍는다 → 시나리오의 "4.0초에 문 열림"과 로그 시각이 그대로 맞는다
- 결과 요약(`결과`, `최종 차량 상태`)은 로그가 아니라 `printf`로 찍은 이유 → 결과 요약은 **프로그램이 만들어 내는 답(출력)**이고, 로그는 **답을 만드는 과정의 기록**이다. 로그 레벨을 `warn`이나 `off`로 바꿔도 **답은 항상 나와야** 한다. 로그로 찍으면 레벨에 따라 결과가 사라져 버린다
  - 웹으로 치면 API의 **응답 바디**(항상 보냄)와 서버의 **로그 파일**(레벨에 따라)의 차이
  - 나중에 E2E 테스트(M5-10)에서 결과 요약을 다른 프로그램이 읽어 검사할 때도, 로그 설정과 상관없이 항상 같은 형식으로 나와야 한다

## 3. 가변 인자 매크로

```c
#define LOG_INFO(...)  log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)

LOG_INFO("차속  %s", buf);
→ log_write(LOG_LEVEL_INFO, "src/log_reader.c", 64, "차속  %s", buf);
```

- `...`과 `__VA_ARGS__`의 뜻 →
  - 매크로 정의의 `...` = **"인자를 몇 개든 받겠다"** (가변 인자)
  - `__VA_ARGS__` = **"받은 인자를 전부 그대로 여기에 붙여 넣어라"** (VA = Variable Arguments)
  - 그래서 `LOG_INFO("a")`(1개), `LOG_INFO("%u", x)`(2개), `LOG_INFO("%s %u", s, x)`(3개)를 전부 하나의 매크로로 쓸 수 있다. printf처럼 인자 개수가 정해지지 않은 함수를 감쌀 때 쓴다

```
LOG_INFO("줄 %u: %s", n, name)
          └──────────────────┘ ← 이 전체가 __VA_ARGS__
→ log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, "줄 %u: %s", n, name)
```

- 함수가 아니라 매크로로 만든 이유 (M0-7 `ASSERT_EQ`와 같은 이유) → **`__FILE__`, `__LINE__`이 로그를 찍은 그 자리의 파일/줄 번호가 되게** 하려고. 매크로는 전처리기가 **쓴 자리에 글자를 그대로 붙여 넣으니까**(M0-2), `__LINE__`이 `LOG_INFO`를 쓴 줄(64)이 된다. 함수로 만들면 `__LINE__`이 항상 `log_write` 안쪽 줄 번호로 고정돼서, DEBUG 로그의 `(log_reader.c:48)` 같은 위치 정보가 쓸모없어진다
  - 덤: 레벨을 매크로 이름에 넣어서(`LOG_WARN`) 쓰는 쪽이 짧고 읽기 쉽다
- `log_write` 안의 `va_list`, `va_start`, `vfprintf`, `va_end`가 하는 일 → 함수 쪽에서 `...`로 받은 인자를 꺼내 쓰는 도구들 (`<stdarg.h>`)

| 이름 | 하는 일 |
| --- | --- |
| `va_list args;` | 가변 인자 목록을 가리킬 **변수** (어디까지 읽었는지 기억하는 책갈피) |
| `va_start(args, fmt);` | 목록 읽기 **시작**. "마지막 고정 인자(`fmt`) 다음부터가 가변 인자"라고 알려 준다 |
| `vfprintf(out, fmt, args);` | `fprintf`인데 인자를 `...` 대신 **`va_list`로 받는 버전**. 받은 인자를 형식대로 찍는다 |
| `va_end(args);` | 다 썼으니 **정리** (`va_start`와 짝) |

  - `fprintf(out, fmt, ...)`로 바로 못 넘기는 이유: 함수 안에서는 받은 `...`을 다른 함수의 `...`로 "그대로" 넘기는 문법이 없다. 그래서 `v`가 붙은 버전(`vfprintf`, `vsnprintf`)이 따로 있다
  - Java로 치면 `void log(String fmt, Object... args)`의 `args` 배열을 `String.format(fmt, args)`에 넘기는 것과 같다. C는 배열 대신 `va_list`라는 특별한 도구를 쓴다

## 4. `__attribute__((format(printf, 4, 5)))`

- 이게 뭔가? → 컴파일러에게 **"이 함수는 printf처럼 동작한다"**고 알려 주는 표시. 그러면 컴파일러가 printf에 해 주는 검사(형식 `%u`, `%s`와 인자 타입이 맞는지, 개수가 맞는지)를 우리 `log_write`에도 해 준다
  - `format(printf, 4, 5)`: printf 형식이고, **4번째 인자가 형식 문자열**(`fmt`), **5번째부터가 가변 인자**(`...`)

```c
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);
//             1번째          2번째             3번째     4번째 ← 형식     5번째~ ← 값들
```

  - gcc/clang 전용 확장이라 `#if defined(__GNUC__)`로 감쌌다 (다른 컴파일러에서는 그냥 빠진다)
- 이게 없으면 놓치는 실수 (예: `LOG_INFO("%u", "문자열")`) → 직접 해 봤다

```c
LOG_INFO("차속 %u km/h", "문자열");   /* %u(숫자)인데 문자열을 넘김 */
LOG_INFO("%s %s", "하나만");          /* %s가 2개인데 값은 1개 */
```

| | 결과 |
| --- | --- |
| attribute **있음** | `warning: format specifies type 'unsigned int' but the argument has type 'char *'` / `warning: more '%' conversions than data arguments` → 우리는 `-Werror`라 **빌드 실패**로 바로 잡힘 |
| attribute **없음** | 경고 **0개**. 그냥 컴파일되고, 실행하면 이상한 숫자가 찍히거나 없는 인자를 읽어서 **UB(정의되지 않은 동작)** → 죽을 수도 있다 |

  - `...`(가변 인자)으로 받으면 C는 원래 **타입을 전혀 검사하지 못한다** (몇 개가 어떤 타입으로 왔는지 함수가 모름). printf는 컴파일러가 특별히 알고 있어서 검사해 주는데, 우리가 만든 `log_write`는 모른다 → 그래서 알려 줘야 한다
  - 로그는 **문제가 생겼을 때 보는 것**인데, 로그 찍는 줄에서 프로그램이 죽으면 최악이다. 평소에 잘 안 도는 WARN/ERROR 경로일수록 이런 실수가 숨어 있기 쉽다 → 컴파일할 때 잡는 게 중요하다

## 5. 로거의 상태 (`static` 전역 변수)

```c
static LogLevel s_level = LOG_LEVEL_INFO;
static FILE    *s_out = NULL;
static uint32_t s_time_ms = 0u;
```

- 전역 변수를 쓴 이유와 `static`을 붙인 이유 →
  - **전역 변수를 쓴 이유:** 로그는 프로그램 **어디서나** 찍는다 (`log_reader.c`, `main.c`, 나중에는 M4 상태 머신에서도). 레벨이나 출력 위치를 함수마다 인자로 넘기면, 로그를 찍는 모든 함수에 `LogLevel level, FILE *out`을 달아야 한다 (M3-3의 `FILE *err` 매개변수가 그랬다). 프로그램 전체에 로거는 **하나**면 충분하니, 설정을 한 곳에 두고 다들 같은 걸 쓴다
    - Java로 치면 `LoggerFactory.getLogger()`가 돌려주는 공용 로거나, **싱글톤(singleton)** 객체와 같다
  - **`static`을 붙인 이유:** 함수 밖 변수에 `static`을 붙이면 **이 파일(`logger.c`) 안에서만** 보인다 (M0-7: 파일 안의 `static` 함수 = Java `private`). 다른 파일이 `s_level = 99;`처럼 **직접 바꿀 수 없다.** 바꾸려면 반드시 `log_set_level()`을 거쳐야 한다
    - Java로 치면 `private static` 필드 + `setLevel()` 같은 setter. 값을 마음대로 못 바꾸게 막고 정해진 입구로만 바꾸게 하는 **캡슐화**
    - `s_` 접두사는 "static 변수"라는 표시 (팀마다 다른 관례)
  - 함수 **안**의 `static`은 뜻이 다르다 (값이 함수 호출 사이에도 유지됨, 예: M2-6 `samples[]`). 같은 키워드가 놓인 곳에 따라 뜻이 바뀐다
- 테스트에서 이 상태 때문에 조심할 점 → 전역 상태는 **테스트 사이에 남는다.** 한 테스트에서 `log_set_level(LOG_LEVEL_WARN)`을 하면, 다음 테스트도 WARN인 채로 시작한다 → 다음 테스트가 INFO 로그를 기대하면 우연히 실패하거나, 테스트 순서를 바꾸면 결과가 달라진다
  - 그래서 `test_logger.c`는 **테스트마다 맨 앞에서 레벨과 시각을 직접 설정**한다 (`log_set_level(...)`, `log_set_time_ms(...)`)
  - 출력 위치도 `capture_end()`에서 `log_set_output(NULL)`로 **원래대로 되돌린다**
  - `test_log_reader.c`는 시작할 때 `log_set_level(LOG_LEVEL_OFF)`로 로그를 끈다 → 테스트 출력이 로그로 지저분해지지 않게
  - 전역 상태의 단점이 바로 이것이다. Java에서 static 필드나 싱글톤을 테스트할 때 `@BeforeEach`로 초기화하는 것과 같은 문제

## 6. "값이 바뀔 때만" INFO

- `log_read_stream`에서 `VehicleInputs before = *inputs;`를 하는 이유 → **"바뀌었나?"를 알려면 바뀌기 전 값이 있어야** 하기 때문. `can_dispatch()`가 `inputs`를 직접 고쳐 버리므로, 부르기 **전에** 사진을 찍듯 복사본(`before`)을 남겨 둔다. 그다음 `before`와 새 `inputs`를 비교해서 다르면 INFO를 찍는다

```c
VehicleInputs before = *inputs;                        /* 1) 바뀌기 전 사진 */
DispatchResult dr = can_dispatch(&frame, inputs, ...); /* 2) inputs가 새 값으로 바뀜 */
log_dispatch(&frame, dr, &before, inputs);             /* 3) 전/후 비교 → 다르면 INFO */
```

  - `*inputs`로 구조체를 대입하면 **내용 전체가 복사**된다 (M0-3 실험 4: C 구조체 대입 = 통째 복사). Java처럼 같은 객체를 가리키는 게 아니라서 `before`는 디스패치 후에도 옛날 값 그대로다
  - `!before->speed_received`도 같이 본다 → **처음 받은 값**은 "바뀐 것"이 아니어도 한 번은 찍는다 (0초의 `차속 0.00 km/h`)
- `memcmp`로 구조체를 통째로 비교하지 않고 필드별로 비교한 이유 (M0-3 패딩) → 구조체 안에는 **패딩(빈 바이트)**이 있을 수 있는데, 그 바이트의 값은 정해져 있지 않다. `memcmp`는 패딩까지 바이트 단위로 비교하므로, **값은 같은데 패딩 쓰레기가 달라서 "다르다"고 나올** 수 있다
  - 실제로 `VehicleSpeed { bool valid; uint16_t centi_kmh; }`는 확인해 보니 `sizeof` = **4** (`valid` 1바이트 + **패딩 1바이트** + `centi_kmh` 2바이트). `uint16_t`가 2의 배수 주소에 앉아야 해서 `valid` 뒤에 빈 칸이 생긴다

```
바이트:   0       1       2   3
        [valid][ 패딩 ][centi_kmh]
                  ↑ 이 바이트는 아무 값이나 들어 있을 수 있음
```

  - 디코더는 임시 변수 `VehicleSpeed s;`(초기화 안 함)에 값을 채워 복사하므로, 패딩 바이트에는 **스택에 남아 있던 쓰레기**가 같이 따라온다 → `memcmp`로 비교하면 같은 60 km/h인데도 "바뀜"으로 판단해 매 프레임 INFO가 찍힐 수 있다
  - 그래서 `speed_changed()`, `doors_changed()`, `ign_changed()`는 **의미 있는 필드만** 하나씩 비교한다. Java의 `equals()`를 필드별로 직접 만드는 것과 같다 (Java는 패딩을 신경 쓸 일이 없다)

## 7. API 변경

- M3-3의 `log_read_file(path, &in, &stats, stderr)`에서 `FILE *err`가 빠진 이유 → **"오류를 어디에 찍을지"를 로거가 맡게 됐기 때문.** M3-3에서는 로거가 없어서, 잘못된 줄을 알릴 곳을 호출하는 쪽이 매번 넘겨줬다 (`stderr`, 테스트에서는 `NULL`). 이제는 `LOG_WARN(...)`이 알아서 로거 설정(레벨, 출력 위치)을 따르니 매개변수가 필요 없다

| | M3-3 (전) | M3-5 (후) |
| --- | --- | --- |
| 호출 | `log_read_file(path, &in, &stats, stderr)` | `log_read_file(path, &in, &stats)` |
| 오류 출력 | `if (err != NULL) fprintf(err, "줄 %u: ...")` | `LOG_WARN("줄 %u: ...")` |
| 테스트에서 끄기 | 매번 `NULL`을 넘김 | 처음에 `log_set_level(LOG_LEVEL_OFF)` 한 번 |
| 레벨 구분 | 없음 (찍거나 안 찍거나) | WARN / ERROR / INFO / DEBUG |

  - `FILE *err`가 남아 있으면, 나중에 M4 상태 머신이나 M5 소켓 코드에서 로그를 찍을 때마다 **모든 함수에 `err`를 줄줄이 넘겨야** 한다 → 로거로 모아서 그 줄줄이를 없앴다 (5번 "전역 변수를 쓴 이유")
  - API를 바꾸면 **부르는 쪽도 같이 고쳐야** 한다 → `main.c`와 `test_log_reader.c`를 함께 고쳤고, 테스트가 전부 통과해서 바꾼 뒤에도 동작이 같다는 걸 확인했다 (M2-2의 리팩터링과 같은 방식)

## 8. 테스트

- 결과 → `[test_logger]` **17 passed**, 0 failed (맥, 컨테이너, Sanitizer + `-Wformat=2` 엄격 경고 모두 통과)

| 테스트 | 확인하는 것 |
| --- | --- |
| `test_format` | `[   1.800s] INFO  차속 16.00 km/h` 형식이 글자 하나까지 정확한지 |
| `test_level_filter` | WARN으로 두면 DEBUG/INFO는 안 찍히고 WARN/ERROR만 찍히는지 |
| `test_level_off` | OFF면 ERROR도 안 찍히는지 |
| `test_debug_has_file_line` | DEBUG 로그에 `(test_logger.c:줄)`이 붙는지 (`__FILE__`, `__LINE__`) |
| `test_parse_level` | `"debug"`, `"warn"`, `"off"` → 레벨, `"verbose"`, `"INFO"`(대문자), NULL → 실패 |

- 로그 출력을 어떻게 테스트했나 (`log_set_output` + `tmpfile`) → 로그는 화면에 찍히는 거라 그냥은 결과를 검사할 수 없다. 그래서 **출력 위치를 임시 파일로 바꿔서** 찍힌 글자를 다시 읽어 와 비교했다

```c
capture_begin();                 /* tmpfile()을 만들고 log_set_output(tmp) */
LOG_INFO("차속 %u.%02u km/h", 16u, 0u);
capture_end(buf, sizeof(buf));   /* rewind → fread로 buf에 읽기 → 출력 위치 되돌리기 */
ASSERT_TRUE(strcmp("[   1.800s] INFO  차속 16.00 km/h\n", buf) == 0);
```

  - `log_set_output()`을 만들어 둔 덕분에 가능했다 → **테스트하기 쉽게 설계**한 것 (M3-3 `log_read_stream`이 `FILE *`를 받게 한 것과 같은 생각)
  - Java로 치면 Logback의 `ListAppender`를 붙여서 찍힌 로그를 리스트로 받아 검사하는 것과 같다
- 잘못된 줄의 WARN 로그 시각이 그 줄의 시각이 아니라 **이전 프레임 시각**으로 찍히는 이유 → 로그 시각은 `log_set_time_ms()`로 바꾸는데, 이건 **파싱에 성공한 뒤에만** 부른다 (`log_read_stream`의 3단계). 잘못된 줄은 파싱에 실패했으니 **그 줄의 타임스탬프를 알 수 없다** → 시각이 안 바뀌고 직전에 성공한 프레임의 시각이 그대로 남아 있다

```
줄 56: (1.700000) vcan0 100#B80B...   ← 파싱 성공 → log_set_time_ms(1700)
줄 57: (1.710000) vcan0 100#10G7...   ← 파싱 실패 → 시각을 못 읽음 → 여전히 1700
       → [   1.700s] WARN  줄 57: ERR_DATA: (1.710000) vcan0 100#10G7...
```

  - 타임스탬프 부분만 멀쩡하면 읽을 수도 있지만, `garbage line`처럼 **타임스탬프 자체가 깨진 줄**도 있어서 일관되게 "마지막으로 알던 시각"을 쓴다
  - 대신 WARN 메시지에 **줄 번호와 원래 줄 내용**을 그대로 찍어서, 실제 시각(`1.710000`)은 메시지 안에서 볼 수 있다
  - "로그 시각 = 시뮬레이션이 마지막으로 알고 있는 시각"이라고 이해하면 된다. 알려진 한계로 남겨 둔다
