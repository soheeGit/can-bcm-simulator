# 19. 파일 읽기 루프 (M3-3)

> 코드: `include/log_reader.h`, `src/log_reader.c`, `tests/test_log_reader.c`, `src/main.c`
> 예시 파일: `data/examples/mixed.log` (정상 줄 + 잘못된 줄 섞음)
> 실행: `make && ./build/bcm_sim data/examples/mixed.log`

## 1. 실행 결과

```
can-bcm-simulator v0.0.1
파일: data/examples/mixed.log

줄 9: ERR_DATA: (0.200000) vcan0 100#10G7
줄 10: ERR_TIMESTAMP: vcan0 200#01
줄 11: ERR_UNSUPPORTED: (0.250000) vcan0 100#R
줄 12: ERR_TOO_LONG (127자 초과)

결과
  read 16, ok 7, error 4  (빈 줄/주석 5)
  디스패처: ok=5 ignored=1 error=1

최종 차량 상태
  차속 : 100.00 km/h
  도어 : FL=1 FR=0 RL=0 RR=0 TRUNK=0
  시동 : IGN=ON GEAR=D
```

- 잘못된 줄 4개가 있는데도 끝까지 실행된 이유 →
  - **왜:** 차량이 중간에 멈추면 안 되기 때문이다 ✅ 깨진 줄 하나 때문에 나머지 정상 데이터를 다 버리면 안 된다 (실차 로그에는 잡음 섞인 줄이 있을 수 있다)
  - **어떻게:** 파서가 오류를 돌려주면 루프가 `줄 N: 이유`를 stderr에 찍고 **`continue`로 다음 줄로 넘어간다.** 프로그램을 끝내는(`return`, `exit`) 코드가 없다
- `read 16, ok 7, error 4 (빈 줄/주석 5)` 숫자를 직접 세어 확인해 보기 (16 = 7 + 4 + 5) →

| 줄 번호 | 내용 | 분류 |
| --- | --- | --- |
| 1, 5, 8, 13 | `#` 주석 | 빈 줄/주석 |
| 4 | 빈 줄 | 빈 줄/주석 |
| 2, 3, 6, 7, 14, 15, 16 | 정상 프레임 | **ok 7** |
| 9 | `100#10G7` | error (ERR_DATA) |
| 10 | 타임스탬프 없음 | error (ERR_TIMESTAMP) |
| 11 | `100#R` | error (ERR_UNSUPPORTED) |
| 12 | 너무 긴 줄 | error (ERR_TOO_LONG) |

  - 빈 줄/주석 **5** + ok **7** + error **4** = **16** ✅
  - "ok 7"은 **파싱에 성공한** 줄 수다. 그 7개를 디스패처가 다시 나눈 게 `ok=5 ignored=1 error=1`
    - ignored 1 = 7번 줄 `555#AABB` (관심 없는 ID)
    - error 1 = 14번 줄 `100#10` (파싱은 됐지만 DLC 1이라 차속 디코더가 거부, M3-1 노트 6번의 "값/의미 오류")
- 없는 파일을 넣으면? (`./build/bcm_sim nope.log`) → `파일을 열 수 없습니다: nope.log (No such file or directory)`를 찍고 **종료 코드 1**로 끝난다. `fopen`이 NULL을 돌려주면 `errno`에 이유가 담기고, `strerror(errno)`가 그걸 글자로 바꿔 준다

## 2. 전체 흐름

```
파일 ──fopen──→ FILE* ──fgets(한 줄)──→ parse_candump_line() ──→ can_dispatch() ──→ VehicleInputs
                                          (M3-2)                   (M2-6)
                ──fclose──
```

- M3-2 파서, M2-6 디스패처와 이 루프의 관계 → 이 루프는 **연결하는 역할**만 한다. 파일에서 한 줄씩 꺼내서 파서에 넣고, 파서가 만든 `CanFrame`을 디스패처에 넣는다. 해석(파서)과 판단 전 정리(디스패처)는 이미 만든 부품이 하고, 루프는 **"줄 하나씩 흘려보내기 + 오류 세기 + 시각 계산"**만 맡는다

| 부품 | 하는 일 | 만든 곳 |
| --- | --- | --- |
| 이 루프 (`log_read_stream`) | 파일 열기/읽기/닫기, 줄 번호 세기, 상대 시각 계산, 오류 출력 | M3-3 |
| 파서 (`parse_candump_line`) | 글자 한 줄 → `CanFrame` + 시각 | M3-2 |
| 디스패처 (`can_dispatch`) | `CanFrame` → `VehicleInputs` 갱신 | M2-6 |

  - 그래서 M5에서 입력이 vcan0 소켓으로 바뀌어도, 파서만 소켓 수신으로 바꾸면 되고 **디스패처 이후는 그대로** 쓴다
- 웹으로 치면? → **배치(batch) 작업.** CSV 파일을 한 줄씩 읽어서(`BufferedReader.readLine()`) 파싱하고 → 서비스 로직에 넘기고 → 잘못된 줄은 로그에 남기고 건너뛰는 Spring Batch의 `ItemReader → ItemProcessor → ItemWriter` 흐름과 같다
  - `ItemReader` = `fgets` 루프 / `ItemProcessor` = 파서 / `ItemWriter` = 디스패처
  - Spring Batch의 skip 정책(잘못된 줄은 건너뛰고 개수 세기)도 같은 생각이다

## 3. 파일 다루기: `fopen` / `fgets` / `fclose`

| 함수 | 하는 일 | Java로 치면 |
| --- | --- | --- |
| `fopen(path, "r")` | 파일을 **읽기 모드(`"r"`)**로 연다. 성공하면 `FILE *`, 실패하면 `NULL` | `new BufferedReader(new FileReader(path))` |
| `fgets(buf, size, f)` | 한 줄을 읽어 `buf`에 넣는다. **최대 `size - 1`글자** + 끝에 `'\0'`. 줄바꿈 `'\n'`도 같이 들어온다. 더 읽을 게 없으면 `NULL` | `reader.readLine()` (단, Java는 `'\n'`을 빼 주고 길이 제한이 없음) |
| `fclose(f)` | 파일을 닫고 자원을 돌려준다 | `reader.close()` (`try-with-resources`가 자동으로 해 주는 것) |
| `FILE *` | 열린 파일을 가리키는 **손잡이**. 어디까지 읽었는지 등을 C 라이브러리가 안에 들고 있다. 우리는 이 포인터만 넘기면 된다 | `Reader` 객체 |

  - `fgets`의 `size`가 중요한 이유: 버퍼 크기를 알려줘서 **절대 넘치지 않게** 한다 (M0-5 `snprintf`와 같은 이유). 크기를 안 받는 옛날 함수 `gets()`는 버퍼 오버플로우 때문에 C11에서 **아예 삭제**됐다
  - 그래서 `line[LOG_LINE_MAX]`(128칸)에 `fgets(line, sizeof(line), in)` → 127글자까지만 들어온다
- `fopen`이 실패하면 무엇을 돌려주나? 이유는 어디서 알 수 있나 (`errno`, `strerror`) → **`NULL`**을 돌려준다. 실패 **이유**는 전역 변수 **`errno`**에 숫자로 담기고, **`strerror(errno)`**가 그 숫자를 사람이 읽는 글자로 바꿔 준다

```
fopen("nope.log", "r")   → NULL
errno                     → 2 (ENOENT)
strerror(errno)           → "No such file or directory"
→ 출력: 파일을 열 수 없습니다: nope.log (No such file or directory)
```

  - 다른 이유 예: 권한 없음 → `"Permission denied"`, 폴더를 열려고 함 → `"Is a directory"`
  - Java는 `FileNotFoundException` 예외에 이유가 들어 있지만, C는 **반환값(NULL)으로 실패를 알리고, 이유는 errno로 따로** 알려준다. 반환값을 확인하지 않으면 실패를 모른다 → NULL 포인터로 `fgets`를 부르면 Segfault (M0-3)
  - 주의: `errno`는 **바로 다음 함수가 덮어쓸 수 있다** → 실패한 직후에 바로 읽어야 한다
- `fclose`를 안 하면? →
  - **자원 누수:** 프로그램이 동시에 열 수 있는 파일 수에는 한계가 있다 (보통 수백~수천 개). 닫지 않고 계속 열면 결국 `fopen`이 실패한다 (`"Too many open files"`). M0-6 메모리 누수의 파일 버전
  - **쓰기 내용 유실:** 쓰기 모드라면 C 라이브러리가 데이터를 **모아뒀다가** 한 번에 쓰는데(버퍼링), 닫지 않으면 마지막 내용이 파일에 안 써질 수 있다 (M3-3에서 `fflush(stdout)`을 쓴 것도 같은 버퍼링 때문)
  - 프로그램이 끝나면 OS가 대부분 정리해 주지만, 차량 제어기처럼 **오래 켜져 있는 프로그램**에서는 쌓여서 문제가 된다 → 그래서 `goto cleanup`으로 반드시 닫는다 (5번)

## 4. `fgets`의 함정: 줄이 버퍼보다 길 때

- `fgets(line, 128, in)`인데 줄이 200자면 어떻게 되나? → `fgets`는 **127글자까지만** 읽고 멈춘다 (마지막 1칸은 `'\0'` 자리). 줄의 나머지 73글자는 **파일에 그대로 남아 있다.** 그래서 다음 `fgets`를 부르면 그 73글자를 **새 줄처럼** 읽어 버린다

```
파일:  [ 200글자짜리 한 줄 ][\n][ 다음 줄 ][\n]

1번째 fgets → 1 ~ 127번째 글자       (줄이 안 끝났는데 멈춤)
2번째 fgets → 128 ~ 200번째 글자 + \n  ← 같은 줄의 뒷부분을 "다음 줄"로 오해!
3번째 fgets → 다음 줄
```

  - 한 줄이 **두 줄로 쪼개져** 줄 번호도 틀리고, 뒷부분이 이상한 줄로 파싱된다
- 그걸 어떻게 알아채나? (`line[len - 1] != '\n'`) → 정상적으로 한 줄을 다 읽었으면 **마지막 글자가 `'\n'`**이다. 그런데 버퍼가 **꽉 찼는데**(127글자) 마지막이 `'\n'`이 아니면, 줄이 아직 안 끝났다는 뜻이다

```c
size_t len = strlen(line);
if (len == sizeof(line) - 1u        /* 버퍼가 꽉 찼고 (127글자) */
    && line[len - 1u] != '\n'       /* 마지막이 줄바꿈이 아니고 */
    && !feof(in)) {                 /* 파일 끝도 아니면 → 너무 긴 줄 */
```

- `discard_rest_of_line()`이 없으면 무슨 일이? (`test_read_too_long_line`) → 위 그림처럼 **긴 줄의 뒷부분이 다음 줄로 읽힌다.** 그래서 "너무 긴 줄"이라고 알아챈 다음, `'\n'`이 나올 때까지 **나머지 글자를 읽어서 버린다** → 다음 `fgets`는 진짜 다음 줄부터 시작한다. 테스트에서 200자 줄 + 정상 줄을 넣었을 때 **줄 수가 2**(3이 아님)이고 다음 정상 줄이 제대로 읽히는지 확인한다
- 마지막 줄에 `'\n'`이 없는 경우와 구분하는 방법 (`feof`) → 파일의 **마지막 줄**은 끝에 `'\n'`이 없을 수 있다 (편집기에 따라 다름). 이때도 "마지막이 `'\n'`이 아님"이 되므로, 너무 긴 줄과 헷갈릴 수 있다
  - 구분 방법 1: **버퍼가 꽉 찼는지** (`len == 127`). 마지막 줄이 짧으면(정상 줄은 60자 정도) 버퍼가 안 찼으니 긴 줄이 아니다
  - 구분 방법 2: **`feof(in)`** — `fgets`가 읽다가 파일 끝에 닿았으면 `feof`가 true. 파일 끝이면 "남은 글자"가 없으니 긴 줄로 보지 않는다
  - `test_read_last_line_without_newline`이 이걸 확인한다
  - 한계: 정확히 127글자인 마지막 줄이 `'\n'` 없이 끝나면 너무 긴 줄로 셀 수 있다 (`fgets`가 127글자에서 멈추면 아직 파일 끝을 "확인"하기 전이라 `feof`가 false). 정상 줄은 60자 정도라 실제로는 생기지 않는다

## 5. `goto cleanup` — C의 try-finally

```c
FILE *in = fopen(path, "r");
if (in == NULL) {
    goto cleanup;
}
...
cleanup:
    if (in != NULL) {
        fclose(in);
    }
    return result;
```

- C에 try-finally가 없어서 생기는 문제 → 자원(파일, 메모리 등)을 열었으면 **어떤 길로 빠져나가든** 반드시 닫아야 한다. Java는 `try-with-resources`나 `finally`가 알아서 닫아 주지만, C는 `return`하는 **모든 곳마다** 직접 `fclose`를 써야 한다. 실패하는 곳이 여러 군데면 닫는 코드가 여기저기 복사되고, 하나만 빼먹어도 **파일이 안 닫힌 채 남는다** (자원 누수, M0-6 메모리 누수와 같은 종류)

```c
/* goto 없이 쓰면 */
FILE *in = fopen(path, "r");
if (in == NULL) { return -1; }
if (조건1) { fclose(in); return -1; }    /* 여기도 닫고 */
if (조건2) { fclose(in); return -1; }    /* 여기도 닫고 */
...
fclose(in);                               /* 여기도 닫고 → 하나 빼먹으면 누수 */
return 0;
```

- `goto cleanup`이 해결하는 것 → 실패하든 성공하든 **전부 `cleanup:` 한 곳으로 모여서** 정리한다. 닫는 코드가 **딱 한 번**만 있으니 빼먹을 수가 없다. Java의 `finally` 블록을 손으로 만든 것과 같다

```c
FILE *in = NULL;
in = fopen(path, "r");
if (in == NULL) { goto cleanup; }   /* 실패 → 정리 장소로 */
result = log_read_stream(...);      /* 성공해도 그대로 아래로 내려감 */
cleanup:                            /* ← finally */
    if (in != NULL) { fclose(in); } /* 열렸을 때만 닫기 */
    return result;
```

  - `in = NULL`로 시작하는 이유: 파일을 열기 **전에** 실패해도 `cleanup`에서 `in != NULL` 검사로 안전하게 건너뛸 수 있게
- `goto`는 나쁘다고 하지 않나? 이 경우는 괜찮은 이유 (MISRA-C의 입장) →
  - `goto`가 나쁘다는 건 **아무 데로나 뛰어서** 코드 흐름을 알 수 없게 만들 때다 (스파게티 코드)
  - `goto cleanup`은 **함수 맨 아래의 한 곳으로, 앞으로(아래로)만** 뛴다 → 흐름이 단순하다. 리눅스 커널 같은 큰 C 프로젝트에서 표준처럼 쓰는 관용구다
  - **MISRA-C:2012**: Rule 15.1 "goto는 쓰지 않는 게 좋다"는 **권고(advisory)**라서 팀이 근거를 남기고 예외로 허용할 수 있다. 대신 Rule 15.2 "goto는 **같은 함수 안, 뒤쪽 라벨로만**", Rule 15.3 "같은 블록이나 바깥 블록의 라벨로만"은 **필수(required)**다. `goto cleanup`은 이 필수 규칙을 지킨다
  - goto를 아예 금지하는 팀이라면 → 함수를 쪼개거나(`log_read_file`이 열기/닫기만 하고 실제 처리는 `log_read_stream`에 맡김) `if`를 중첩해서 출구를 하나로 만든다. 사실 지금 코드도 이미 두 함수로 나눠져 있어서 자원이 하나뿐이라 goto 없이도 쓸 수 있는데, **자원이 여러 개(파일 + 소켓 + 메모리)로 늘어날 때를 대비한 관용구**를 연습한 것이다 (M5)

## 6. 상대 시각

- `(1697800000.250000)` → `250 ms`가 되는 과정 → **첫 프레임의 시각을 0으로 잡고, 그 뒤 프레임은 "첫 프레임에서 얼마나 지났나"로 바꾼다**

```
첫 줄:  (1697800000.000000) → 파서: 1697800000000000 us → first_ts_us로 저장 → 0 - 0          = 0 us   → 0 ms
둘째 줄: (1697800000.250000) → 파서: 1697800000250000 us → 1697800000250000 - 1697800000000000 = 250000 us → 250 ms
                                                           (빼기)                                   (÷ 1000)
```

  - 1단계 (M3-2 파서): 글자 → **마이크로초** 정수 (`uint64_t`, 큰 숫자)
  - 2단계 (이 루프): 첫 프레임 시각을 **빼서** 상대 시각으로 → 작은 숫자가 된다
  - 3단계: ÷ 1000 → **밀리초** → `uint32_t t_ms`로 디스패처에 넘긴다 (디스패처와 M4 타임아웃은 ms 단위)
  - 왜 빼나: 디스패처의 `now_ms`는 `uint32_t`라 1697800000250 ms 같은 큰 값은 안 들어간다 (약 43억까지). 상대 시각이면 약 49일까지 담을 수 있어 충분하다. 그리고 시나리오 파일(0초부터)과 실제 candump 파일이 **같은 기준**이 된다
  - 첫 프레임 기준이라 **빈 줄이나 주석, 잘못된 줄은 기준이 되지 않는다** (파싱에 성공한 첫 줄이 0ms)
- 시간이 거꾸로 가면(뒤 줄의 시각이 더 작으면) 어떻게 처리했나 → **0ms로 처리한다** (`ts_us >= first_ts_us ? 차이 : 0`)
  - 그냥 빼면 unsigned끼리 빼기라 음수가 안 되고 **엄청 큰 값으로 넘어간다** (M0-4 실험 2: `3 - 5 = 4294967294`). 그러면 "수십억 ms가 지났다"로 오해해서 M4에서 모든 신호가 타임아웃으로 판단될 수 있다
  - 로그를 잘못 합치거나 시계가 뒤로 바뀌면 생길 수 있는 상황이다. 지금은 단순하게 0으로 막아 두고, 경고를 찍을지는 M3-5 로그 레벨에서 다시 정한다

## 7. 테스트

| 테스트 | 확인하는 것 |
| --- | --- |
| `test_read_all_good` | 정상 3줄 → 전부 OK, 차속/도어/기어가 반영됨 |
| `test_read_mixed_continues_after_errors` | 잘못된 줄 2개가 중간에 있어도 **멈추지 않고** 마지막 정상 줄(100.00 km/h)까지 반영 |
| `test_read_skips_blank_and_comments` | 빈 줄, 주석, 공백만 있는 줄 → skipped로 세고 오류로 안 셈 |
| `test_read_too_long_line` | 200자짜리 줄 → **그 줄만** 오류 1개, 나머지 글자가 다음 줄로 섞이지 않아 다음 정상 줄은 그대로 읽힘 (줄 수 2) |
| `test_read_last_line_without_newline` | 파일 마지막 줄에 `'\n'`이 없어도 정상으로 읽힘 (너무 긴 줄로 오해하지 않음) |
| `test_read_relative_time` | 실제 candump 시각(`1697800000.xxx`) → 첫 프레임 0ms, 두 번째 250ms |
| `test_read_empty` | 빈 파일 → 0줄, 아무 상태도 안 바뀜 |
| `test_read_dispatch_stats` | 파싱은 OK인데 디스패처에서 IGNORED/ERROR인 경우도 따로 셈 |
| `test_file_not_found` | 없는 파일 → `-1` 반환 (프로그램이 죽지 않음) |
| `test_file_example` | 저장소의 `data/examples/mixed.log` → 16줄, 오류 4, 프레임 7, 최종 차속 100.00 km/h |
| `test_null_args` | NULL 인자 → `-1` |

- 결과 → `[test_log_reader]` **45 passed**, 0 failed (맥, 컨테이너, Sanitizer + 엄격 경고 모두 통과)
- 테스트에서 진짜 파일 대신 `tmpfile()`을 쓴 이유 →
  - 테스트마다 **입력 내용을 코드 안에 바로** 적을 수 있다. 테스트를 읽으면 입력과 기대 결과가 한눈에 보인다 (파일을 따로 열어볼 필요 없음)
  - 테스트용 파일을 수십 개 만들어 관리할 필요가 없다
  - `tmpfile()`은 **이름 없는 임시 파일**을 만들고, `fclose`하면 **자동으로 지워진다** → 테스트가 끝나도 쓰레기 파일이 남지 않는다
  - `log_read_stream()`이 파일 이름이 아니라 **`FILE *`를 받게** 나눠 둔 덕분에 가능했다 (테스트하기 쉬운 설계). 진짜 파일 열기는 `test_file_example`, `test_file_not_found`에서 따로 확인한다
  - Java로 치면 테스트에서 실제 파일 대신 `new StringReader("...")`나 `@TempDir`을 쓰는 것과 같다
- 맥의 `fmemopen()`에서 겪은 문제 (트러블슈팅) →
  - **증상:** 처음엔 문자열을 바로 파일처럼 여는 `fmemopen()`을 썼다. 맥에서 `test_read_empty`(빈 문자열)만 실패했다 (`read_text("")`가 -2 반환)
  - **원인:** 맥의 `fmemopen()`은 **크기 0인 버퍼를 열지 못하고 NULL을 돌려준다.** 리눅스에서는 열린다. 같은 POSIX 함수인데 플랫폼마다 동작이 다르다
  - **해결:** `tmpfile()`에 문자열을 쓰고 `rewind()`로 되감아서 읽는 방식으로 바꿨다. 맥과 리눅스 둘 다 같은 결과
  - **배운 점:** C 표준이 아닌 함수(POSIX 확장 등)는 플랫폼마다 다를 수 있다 → 맥과 리눅스 **둘 다**에서 테스트를 돌려야 하는 이유 (그래서 매번 컨테이너에서도 확인한다). 면접에서 "크로스 플랫폼 문제를 겪은 적 있나요?"에 쓸 수 있는 사례
