# 05. 경고 옵션, 디버거, 메모리 검사 도구 (M0-6)

> 실험 코드: `playground/debug/`
> 빌드: `make -C playground/debug` → 같은 코드로 두 가지 실행 파일이 생긴다
> - `build/xxx` : 일반 빌드
> - `build-san/xxx` : 검사 도구(Sanitizer) 포함 빌드
>
> 💡 각 실험은 **일반 빌드 → Sanitizer 빌드** 순서로 실행해서 차이를 비교하기

## 실험 1: 경고 옵션 (`01_warnings.c`)

```bash
make -C playground/debug warn
```

| 코드 속 실수 | 옵션 없음 | `-Wall -Wextra` | `+ -Wconversion -Wshadow` |
| --- | --- | --- | --- |
| (A) int → uint8_t 반환 | ❌ 조용함 | ❌ 조용함 | ⚠️ `implicit conversion loses integer precision` (6줄) |
| (B) 바깥 변수와 같은 이름 | ❌ 조용함 | ❌ 조용함 | ⚠️ `declaration shadows a local variable` (12줄) |
| (C) int → uint16_t 대입 | ❌ 조용함 | ❌ 조용함 | ⚠️ `implicit conversion loses integer precision` (20줄) |
| (D) signed vs unsigned 비교 | ❌ 조용함 | ⚠️ `comparison of integers of different signs` (25줄) | ⚠️ 동일 |

- 실행 결과 [3] `to_byte(300)` → 44 (300 - 256), [4] `speed` → 4464 (70000 - 65536), [5] → `-1 < 3 → false ?!`
- [5]에서 `-1 < 3`이 false가 된 이유 → signed와 unsigned를 비교하면 signed 쪽(-1)이 **unsigned로 바뀐 뒤** 비교된다. -1을 unsigned로 바꾸면 4294967295(M0-4 실험 6)라서 `4294967295 < 3` → false.
- 경고 옵션 없이 빌드하면 무엇이 문제인가? → 4개 실수 모두 **아무 경고 없이 빌드되고 실행도 된다**. 값이 잘리거나(44, 4464), 조건문이 반대로 동작하는데 프로그램은 멀쩡히 끝까지 돈다. 결과가 틀렸다는 걸 알 방법이 없다.
- 우리 프로젝트 Makefile에 `-Werror`를 넣은 이유 → 경고가 떠도 빌드는 되기 때문에, 경고가 쌓이면 무시하게 된다. `-Werror`는 경고를 에러로 바꿔서 **경고가 하나라도 있으면 빌드를 실패**시킨다. 경고를 그냥 넘어가지 못하게 강제하는 장치.
  - 실무 임베디드에서는 `-Wconversion`, `-Wshadow`까지 켜는 경우가 많다 (MISRA-C도 암묵적 형변환을 금지) → M6-2에서 우리 Makefile에도 추가 검토

## 실험 2: 배열 범위 밖 쓰기 (`02_out_of_bounds.c`)

코드 속 실수: `data`는 8칸(0~7번)인데 `i <= 8`이라서 **9번 쓴다** → 9번째(`data[8]`)는 남의 자리

| | 맥 | 컨테이너 (리눅스) |
| --- | --- | --- |
| 일반 빌드 결과 | 출력은 정상, before/after도 멀쩡해 보임 → 마지막에 `Abort trap: 6`으로 종료 | 출력은 정상 → 마지막에 `*** stack smashing detected ***: terminated` |
| Sanitizer 빌드 결과 | `stack-buffer-overflow` + **20번째 줄** | `stack-buffer-overflow` + **20번째 줄** |

- Sanitizer 출력에서 에러 종류와 줄 번호:

```
02_out_of_bounds.c:20:9: runtime error: index 8 out of bounds for type 'uint8_t[8]'
ERROR: AddressSanitizer: stack-buffer-overflow on address ...
SUMMARY: AddressSanitizer: stack-buffer-overflow 02_out_of_bounds.c:20 in main
```

- 일반 빌드에서 `before`, `after` 배열은 망가졌나? → 겉으로는 안 망가졌다. 9번째 바이트가 before/after가 아니라 다른 곳(스택 보호용 값)을 덮어썼다. **어디를 망가뜨릴지는 운**이라, 다른 상황에서는 옆 변수가 조용히 바뀔 수도 있다.
- 일반 빌드가 죽은 시점은 "잘못 쓴 순간"인가, "함수가 끝날 때"인가? → **함수가 끝날 때.** 출력이 다 나온 뒤에 죽었다. 스택 보호 장치(stack canary)는 함수가 끝날 때 "보호용 값이 바뀌었나?"를 검사하기 때문. 어디서 잘못 썼는지는 알려주지 않는다.
- Sanitizer는 언제 잡았나? → **잘못 쓴 바로 그 순간**(20번째 줄 `data[i] = 0xFF;`). 몇 번 칸(index 8)인지까지 알려준다.
- Java라면 어떤 예외가 났을까? → `ArrayIndexOutOfBoundsException: Index 8 out of bounds for length 8`가 그 줄에서 바로 발생. C는 기본적으로 이런 검사가 없어서 Sanitizer를 켜야 Java처럼 잡힌다.

## 실험 3: 정의되지 않은 동작 (`03_ub.c`)

| 코드 | 일반 빌드 출력 | Sanitizer (UBSan) 메시지 |
| --- | --- | --- |
| (A) `1 << 31` | -2147483648 | `03_ub.c:12:17: runtime error: left shift of 1 by 31 places cannot be represented in type 'int'` |
| (B) `INT_MAX + 1` | -2147483648 | `03_ub.c:16:17: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'` |
| (C) `1u << 32` | 1 | `03_ub.c:20:20: runtime error: shift exponent 32 is too large for 32-bit type 'uint32_t'` |

- 일반 빌드는 왜 아무 말도 안 했나? → C는 실행 중에 이런 검사를 하지 않는다. CPU는 시킨 대로 계산만 하고, 결과가 "말이 되는지"는 신경 쓰지 않는다. 그래서 틀린 값을 들고 끝까지 실행된다(`[끝] 프로그램이 끝까지 실행됨`). UBSan은 계산할 때마다 검사 코드를 끼워 넣어서 **파일:줄:칸**까지 알려준다.
- (A), (B)가 둘 다 -2147483648인 이유 → 비트가 똑같기 때문이다.
  - `1 << 31` = `1000 0000 ... 0000`
  - `INT_MAX + 1` = `0111 1111 ... 1111` + 1 → 자리올림이 부호 비트까지 올라가서 `1000 0000 ... 0000`
  - signed int의 맨 위 비트는 자릿값이 -2³¹이므로 -2147483648 (INT_MIN)
- (C) `1u << 32`가 0이 아니라 1이 나온 이유 → 32비트 값을 32칸 이상 미는 것은 UB다. ARM64(맥) CPU의 시프트 명령은 밀 칸 수의 **아래 5비트만** 사용한다(32 = `10 0000` → 아래 5비트 `0 0000` = 0). 그래서 실제로는 `1u << 0` = 1이 계산됐다. 다른 CPU나 컴파일러 최적화에서는 0이 나올 수도 있다. → 결과를 예측할 수 없으니 UB.
- 교훈 → 시프트할 칸 수는 항상 **0 ~ (타입 비트 수 - 1)** 범위인지 확인한다. M2-1 `can_get_bits()`에서 `length`가 32를 넘지 않는지 검사해야 하는 이유.

## 실험 4: use-after-free (`04_use_after_free.c`)

코드 속 실수: `free(p)`로 메모리를 반납한 뒤에 `p->x`를 다시 읽는다

| | 맥 | 컨테이너 |
| --- | --- | --- |
| 일반 빌드 `p->x` 값 | 4 (원래 값 3이 아님) | -1431545440 |
| Sanitizer 결과 | `heap-use-after-free` + **20번째 줄** | `heap-use-after-free` + **20번째 줄** |

```
ERROR: AddressSanitizer: heap-use-after-free on address ...
SUMMARY: AddressSanitizer: heap-use-after-free 04_use_after_free.c:20 in main
```

- 일반 빌드의 값이 맥과 리눅스에서 다른 이유 → 반납한 메모리는 `malloc`을 관리하는 쪽(메모리 관리자)이 **자기 장부 정리용으로 다시 쓴다**. 맥과 리눅스는 관리 방식이 달라서 그 자리에 다른 값이 들어 있다. 어떤 값이 나올지는 정해져 있지 않다 → 쓰레기 값.
- 이 버그가 무서운 이유 →
  - 에러 없이 **끝까지 실행된다** (`[끝] 프로그램이 끝까지 실행됨`)
  - 테스트할 때는 우연히 원래 값이 남아 있어서 통과하고, 나중에 다른 `malloc`이 그 자리를 가져가면 **엉뚱한 데이터를 읽거나 남의 데이터를 덮어쓴다**
  - 원인(free)과 증상(값이 이상함)이 코드상 멀리 떨어져 있어서 찾기 어렵다
- 예방 습관 → `free(p);` 바로 다음에 `p = NULL;` (M0-3 실험 7). 그러면 다시 쓰려 할 때 Segfault로 바로 드러난다. 그리고 임베디드에서는 애초에 `malloc`을 안 쓴다.

## 실험 5: 메모리 누수 (`05_leak.c`)

코드 속 실수: 프레임마다 `malloc(8)`을 하고 `free`를 안 한다 (100번)

| | 맥 | 컨테이너 |
| --- | --- | --- |
| 일반 빌드 | `[끝] 프레임 100개 처리 완료` (아무 문제 없어 보임) | 동일 |
| Sanitizer 결과 | **아무것도 안 잡힘** | `LeakSanitizer: detected memory leaks` |

```
==77==ERROR: LeakSanitizer: detected memory leaks
SUMMARY: AddressSanitizer: 800 byte(s) leaked in 100 allocation(s).
```

- 누수 크기 (몇 바이트, 몇 번 할당) → 8바이트 × 100번 = **800바이트, 100번 할당**
- 맥에서 안 잡힌 이유 → 맥(Apple Silicon)의 AddressSanitizer는 누수 검사(LeakSanitizer)를 지원하지 않는다. `ASAN_OPTIONS=detect_leaks=1`로 강제로 켜도 `detect_leaks is not supported on this platform`이 나온다. LeakSanitizer는 프로그램을 멈추고 메모리 전체를 뒤지는 **리눅스 전용 기능**을 쓰기 때문이다 (그래서 컨테이너에서도 `--privileged`가 필요).
  - 대신 맥에는 Apple의 `leaks` 도구가 있다 (일반 빌드로 실행):

```
$ leaks --atExit -- ./playground/debug/build/05_leak
Process 79021: 100 leaks for 1600 total leaked bytes.
```

  - 리눅스는 800바이트, 맥은 1600바이트 → 맥의 메모리 관리자는 `malloc(8)`도 **최소 16바이트**씩 떼어주기 때문 (16 × 100 = 1600)
- 다른 실험과 다른 점 → 누수는 "잘못된 접근"이 아니라 "반납을 안 한 것"이라서 실행 중에는 아무 일도 없다. LeakSanitizer는 **프로그램이 끝날 때** 반납 안 된 메모리를 세서 알려준다.
- CAN 프레임을 1초에 수천 개 받는 제어기에서 이 버그가 있으면? → 1초에 1000개면 8KB씩, 1분이면 약 480KB가 사라진다. MCU 메모리는 수십~수백 KB라서 **몇 분 만에 메모리가 바닥나고 `malloc`이 실패**한다. 차가 달리는 중에 제어기가 멈출 수 있다. → 그래서 임베디드는 `malloc`을 안 쓰고 고정 크기 버퍼를 미리 잡아둔다.

## 실험 6: 디버거로 버그 찾기 (`06_debug_me.c`)

- 실행 결과: `차속: 41.35 km/h (기대값: 100.00 km/h)`
- 디버거에서 확인한 값

| 변수 | 값 (16진수) |
| --- | --- |
| `lo` | 0x10 |
| `hi` | 0x27 |
| `raw` | 0x1027 |

- 버그 위치와 원인 → **10번째 줄** `(lo << 8) | hi`. 신호 정의서는 Little-endian(앞 바이트 `data[0]`이 작은 자리)인데, 코드는 앞 바이트를 큰 자리로 올리는 **Big-endian 방식으로 조립**했다. 그래서 0x2710(10000)이 아니라 0x1027(4135) → 41.35 km/h가 나왔다.
- 고친 코드 → `uint16_t raw = (uint16_t)((hi << 8) | lo);` → 다시 실행하면 `차속: 100.00 km/h` ✅
- 배운 점 → 문법은 맞는데 **논리가 틀린 버그**는 경고 옵션도 Sanitizer도 못 잡는다. 디버거로 멈춰서 중간 값을 직접 보거나, 기대값을 검사하는 **테스트**가 있어야 잡힌다 (→ M0-7, M2-3 차속 디코더 테스트).

### 자주 쓰는 디버거 명령

| 하고 싶은 것 | lldb (맥) | gdb (리눅스) |
| --- | --- | --- |
| 프로그램 열기 | `lldb ./build/06_debug_me` | `gdb ./build/06_debug_me` |
| 함수에 breakpoint | `b decode_speed_raw` | `break decode_speed_raw` (`b`) |
| 특정 줄에 breakpoint | `b 06_debug_me.c:10` | `break 06_debug_me.c:10` |
| 실행 시작 | `run` (`r`) | `run` (`r`) |
| 다음 줄 (함수 안으로 안 들어감) | `next` (`n`) | `next` (`n`) |
| 다음 줄 (함수 안으로 들어감) | `step` (`s`) | `step` (`s`) |
| 변수 값 보기 | `p raw` | `print raw` (`p`) |
| 지역 변수 전부 보기 | `frame variable` | `info locals` |
| 16진수로 보기 | `p/x raw` | `p/x raw` |
| 호출 경로 보기 (backtrace) | `bt` | `backtrace` (`bt`) |
| 계속 실행 | `continue` (`c`) | `continue` (`c`) |
| 종료 | `quit` (`q`) | `quit` (`q`) |

- 맥 lldb가 `attach failed`로 실행이 안 되면 → 디버깅 권한 문제. `sudo DevToolsSecurity -enable` 후 재시도하거나, 컨테이너 gdb 사용
- `-g` 옵션으로 빌드해야 디버거에서 줄 번호와 변수 이름이 보인다

## 정리: 언제 무엇을 쓰나

| 도구 | 잡는 것 | 언제 잡나 |
| --- | --- | --- |
| 경고 옵션 (`-Wall -Wextra -Wconversion -Wshadow`, `-Werror`) | 값이 잘리는 변환, 이름 겹침, signed/unsigned 비교 등 **위험해 보이는 코드** | **컴파일할 때** (실행 전, 비용 0) |
| AddressSanitizer (`-fsanitize=address`) | 배열 범위 밖 접근, use-after-free | **실행 중, 잘못 접근한 그 순간** |
| UndefinedBehaviorSanitizer (`-fsanitize=undefined`) | signed 오버플로우, 잘못된 시프트 등 UB | **실행 중, 그 계산을 한 순간** |
| LeakSanitizer (리눅스) / `leaks` (맥) | 메모리 누수 (malloc 후 free 안 함) | **프로그램이 끝날 때** |
| 디버거 (lldb / gdb) | 논리 버그 (문법은 맞는데 결과가 틀림) | **내가 원하는 곳에서 멈춰서 직접 확인** |

> 순서: 경고로 먼저 거르고 → `make debug`(Sanitizer)로 실행해서 메모리/UB를 잡고 → 결과가 이상하면 디버거로 추적. 그리고 결과가 맞는지는 **테스트**가 확인한다 (M0-7).
