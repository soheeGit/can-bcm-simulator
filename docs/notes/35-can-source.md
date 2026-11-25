# 35. 입력 소스 추상화: CanSource (M5-5)

> 코드: `include/can_source.h`(인터페이스), `src/file_source.c` (★ 직접 채우기 1), `src/frame_loop.c` (★ 직접 채우기 2), `src/socket_source.c`, `src/log_reader.c`, `src/main.c`, `tests/test_frame_loop.c`
> 맥: `make test` / VM: `make && ./build-linux/bcm_sim -i vcan0`

## 0. 직접 채우기

| 확인 (`make test`) | 결과 |
| --- | --- |
| 채우기 전 | `test_frame_loop` 4 passed, 14 failed / `test_log_reader` 9 passed, 36 failed / `test_scenarios` 24 passed, 48 failed |
| TODO 1만 채운 뒤 | `test_frame_loop` 4 passed, 14 failed (그대로) / `test_log_reader` 17 passed, 28 failed / `test_scenarios` 25 passed, 19 failed |
| TODO 2 첫 시도 `src->read_frame((void *)src, ...)` | `test_log_reader`, `test_scenarios`가 **죽음** (요약 줄도 안 찍힘) |
| TODO 2 두 번째 `src->read_frame(src->self, ...)` (결과 안 받음) | 안 죽음. `test_log_reader` 19 passed, 26 failed / 나머지 그대로 |
| 둘 다 채운 뒤 | ✅ `test_frame_loop` 18 passed / `test_log_reader` 45 passed / `test_scenarios` 140 passed, 전체 `make test` 통과 (맥, VM) |

- TODO 1만 채우고 TODO 2는 비워 두면 어떤 테스트가 실패하나. 왜 → 파일·가짜 소스 테스트가 **전부** 실패한다. 루프가 소스를 한 번도 부르지 않고 `r = CAN_SOURCE_END`("끝")로 바로 나가서, 프레임이 **0개** 처리된다
  - 실패가 **줄어든** 이유: TODO 1 전에는 `read_frame`이 NULL이라 `frame_loop_run`이 `-1`(실패)을 돌려줬다. 이제 `0`(정상 끝)이 돌아와서, 반환값만 보는 assert(`ASSERT_EQ(0, log_read_file(...))` 등)는 통과한다. 내용(줄 수, 차속, 전이)을 보는 assert는 여전히 실패
  - `test_frame_loop`이 그대로인 이유: 이 테스트는 `FileSource`가 아니라 **가짜 소스**(`ArraySource`)를 쓴다 → TODO 1과 상관없음. TODO 2(루프가 소스를 부르는 곳)만 영향
  - 즉 **연결**(TODO 1: 소스를 인터페이스에 끼우기)과 **호출**(TODO 2: 인터페이스로 부르기)이 둘 다 있어야 동작한다
- 채우기 전에 `test_null_args`는 왜 통과했나 → 이 테스트는 "소스가 NULL이거나 `read_frame`이 NULL이면 `-1`"만 본다. 그 검사(`if (src == NULL || src->read_frame == NULL ...)`)는 TODO가 아니라 **이미 써 둔** 코드라서 처음부터 통과
  - 나머지 통과 2개는 `test_reads_until_end`, `test_runs_bcm`의 `ASSERT_EQ(0, frame_loop_run(...))`: 비어 있는 루프도 "끝(END) → 0"을 돌려주니까 우연히 맞음. 짝인 "프레임 수 / 상태" assert가 실패해서 비어 있다는 걸 잡는다 (34번 노트의 짝 테스트와 같다)
  - NULL 검사가 없었다면 `src->read_frame(...)` = **NULL 주소의 함수를 부름** → 세그폴트로 죽는다. 그래서 TODO 1 전에도 죽지 않고 "실패"로 끝났다

## 1. 구조: 무엇을 나눴나

```
M4까지:  log_reader.c = [줄 읽기 + 파싱] + [디스패처 → BCM → 0x400]     ← 파일 전용

M5-5:    FileSource   ─┐
         SocketSource ─┼─ CanSource ──→ frame_loop_run() = [디스패처 → BCM → 0x400]
         (테스트) ArraySource ─┘          (소스가 무엇인지 모름)
```

| Java | 우리 C |
| --- | --- |
| `interface CanSource { Result readFrame(...); }` | `typedef struct { void *self; ... (*read_frame)(void *self, ...); } CanSource;` |
| `class FileSource implements CanSource` | `FileSource` 구조체 + `file_source_read()` + `file_source_as_can_source()` |
| `src.readFrame(f, t)` | `src->read_frame(src->self, &f, &t)` |
| `this` | `void *self` → 구현 안에서 `FileSource *fs = self;`로 되돌린다 |

- `read_frame`만 있으면 되지 왜 `self`도 들고 다니나 (함수 포인터만 넘기면 무엇을 못 하나) → 함수 포인터는 **"무엇을 할지"(코드)**만 가리킨다. **"누구의 데이터로"**는 모른다
  - `file_source_read`는 어느 파일(`fs->in`)을, 몇 번째 줄까지(`fs->lines`) 읽었는지, 첫 시각(`fs->first_ts_us`)이 뭔지 알아야 한다 → 이 **상태**가 `self`(FileSource)에 있다
  - `self` 없이 하려면 상태를 **전역 변수**에 둬야 한다 → 파일 소스를 **하나만** 만들 수 있고, 테스트끼리 상태가 섞인다. `self`가 있으면 `FileSource` 두 개(파일 두 개)를 동시에 써도 각자 따로 읽는다
  - Java로: 메서드(코드)는 클래스에 하나지만 필드(데이터)는 객체마다 있다. `fileA.readFrame()`과 `fileB.readFrame()`은 **같은 코드**를 **다른 this**로 부른다. C는 그 this를 `self`로 직접 넘긴다
  - 함수 포인터 = 메서드, `self` = 객체(필드 묶음). 둘을 같이 들고 다녀야 "객체의 메서드 호출"이 된다
- 이슈에는 `bool read_frame(...)`이라고 돼 있는데 `CanSourceResult`(OK / END / ERROR)로 바꿨다. `bool`이면 무엇을 구분 못 하나 → `false` 하나로는 **"파일이 정상적으로 끝났다"(END)**와 **"읽다가 고장 났다"(ERROR)**를 구분 못 한다
  - 둘의 처리가 다르다: END → `0`(성공, 요약 출력, 종료 코드 0) / ERROR → `-1`(실패, 종료 코드 1). M4까지 `ferror()`로 구분하던 것 (`log_read_stream`의 "읽기 오류")
  - 테스트 `test_source_error_stops`(`-1`)와 `test_reads_until_end`(`0`)가 이 차이를 확인한다
  - 소켓에는 "끝"이 없고 오류만 있다(`read()` 실패). 그래도 같은 인터페이스라 세 가지를 다 둔다
  - Java로: `Iterator.hasNext()`가 `false`(정상 끝)인 것과 `IOException`(고장)이 다른 것과 같다

## 2. 실시간 BCM 실행 (VM)

| 터미널 A | 터미널 B |
| --- | --- |
| `./build-linux/bcm_sim -i vcan0 -o rt.log` | `canplayer -I data/scenarios/02_door_open_while_driving.log vcan0=vcan0` |

끝나면 터미널 A에서 Ctrl+C. 파일 모드(`./build-linux/bcm_sim -f data/scenarios/02_door_open_while_driving.log`)와 비교:

| | 파일 모드 | 실시간 |
| --- | --- | --- |
| STATE 줄 (시각, 규칙) | 1.8s T3 / 4.2s T6 / 5.2s T9 / 9.5s T7 | ✅ 2.879s T3 / 5.279s T6 / 6.279s T9 / 10.579s T7 (+ `도어 잠금 명령` 1번) |
| 전이 사이 간격 | 2.4s / 1.0s / 4.3s | ✅ 2.4s / 1.0s / 4.3s (같음) |
| 0x400 송신 수 | 106 | ✅ 106 (`rt.log` 첫 줄 `(1.080000) ... 400#...00`, 마지막 alive `09`) |

- 실시간 시각이 파일 모드와 다른 이유. 그런데 규칙 순서와 간격은 같은 이유 → **0초의 기준이 다르다**
  - 파일 모드: 0초 = **파일의 첫 프레임** 시각 (`FileSource`가 `first_ts_us`를 뺀다)
  - 실시간: 0초 = **`bcm_sim`을 켠 순간** (`SocketSource`가 소켓을 연 시각 `start_ms`를 뺀다). 켜고 나서 터미널 B에서 `canplayer`를 치기까지 걸린 시간(여기서는 약 1.08초)만큼 전부 뒤로 밀린다
  - 간격이 같은 이유: `canplayer`가 로그의 **시각대로**(0.1초 간격은 0.1초 기다려서) 보내니까 프레임 사이 간격이 파일과 같다. 그리고 판단하는 코드가 **같은 `frame_loop_run()`**이다 → 같은 입력 순서 + 같은 간격 + 같은 코드 = 같은 규칙, 같은 순서
  - 실시간은 수 ms씩 흔들릴 수 있다 (OS 스케줄링). 여기서는 4번 모두 정확히 맞았다
  - 0x400도 106개로 같다 → 송신 주기(100ms)도 프레임 시각 기준이라 똑같이 나왔다. **소스만 바꿨고 판단은 안 바뀌었다** = M5-5의 목표
- `--dump`를 붙였을 때와 안 붙였을 때 차이 (터미널 B: `cansend vcan0 300#0203`, `cansend vcan0 200#00`, `cansend vcan0 100#D007000000000000`)

| | `-i vcan0 -d` (덤프, M5-4) | `-i vcan0` (실시간 BCM, M5-5) |
| --- | --- | --- |
| 하는 일 | 받은 프레임을 **해석해서 찍기만** | 디스패처 → **BCM 판단** → 0x400 |
| 출력 | 프레임마다 `RX 0x300 [2] 02 03 → 시동 IGN=ON GEAR=D` | 값이 **바뀔 때만** `시동 IGN=ON GEAR=D` / `차속 20.00 km/h` + `STATE IDLE -> DRIVING_LOCKED (T3 ...)`, `도어 잠금 명령` |
| 상태 기억 | 없음 (한 프레임씩 따로) | 있음 (VehicleInputs, BcmContext) |
| `-o` | 의미 없음 | 0x400을 파일로 |

  - 덤프는 `candump`처럼 "**무엇이 오는지**" 볼 때, BCM 모드는 "**BCM이 어떻게 판단하는지**" 볼 때 쓴다

## 3. 가짜 소스로 테스트하기 (`tests/test_frame_loop.c`)

- `ArraySource`는 무엇이고, 이게 왜 쉽게 만들어지나 (M4까지 구조였다면 같은 테스트를 어떻게 해야 했나) → **테스트 파일 안에서만 쓰는 세 번째 `CanSource` 구현.** 배열에 넣어 둔 `CanFrame`과 시각을 차례로 꺼내 주고, 다 꺼내면 `END`
  - 쉬운 이유: `frame_loop_run()`은 `read_frame(self, out, ts)` **약속만** 본다. 약속대로 함수 하나 + 구조체 하나(`array_source_read`, `ArraySource`)만 만들면 끼워진다. 루프 코드는 한 줄도 안 바꿨다
  - M4까지 구조였다면: 루프가 `fgets`(파일)에 붙어 있어서, 프레임을 넣으려면 **candump 문자열을 만들어 임시 파일에 써야** 했다 (`test_log_reader.c`의 `read_text()`가 `tmpfile()`로 하는 일). 프레임 → 문자열 → 파싱 → 프레임으로 한 바퀴 돌아야 했고, 파서 버그가 루프 테스트를 깨뜨릴 수도 있다
  - 지금은 `CanFrame`을 **바로** 넣는다 → 루프(디스패처 → BCM → 0x400)만 따로 확인
  - Java로: `CanSource`를 `Mockito.mock()`하거나 테스트용 `FakeCanSource implements CanSource`를 만드는 것. DB 대신 인메모리 저장소를 끼워 서비스만 테스트하는 것과 같다
- 파일로는 만들기 어렵고 가짜 소스로는 쉬운 테스트 하나 (`test_source_error_stops`) → **"읽다가 고장 남"(ERROR)** 상황
  - 파일로 만들려면 `fgets`가 도중에 **입출력 오류**(`ferror`)를 내야 한다. 디스크 고장, 읽는 도중 USB 뽑기 같은 상황이라 정상 파일로는 재현하기 어렵다
  - 가짜 소스는 `fail_at = 1`만 주면 "두 번째에서 ERROR"를 돌려준다 → 루프가 **`-1`로 멈추고**, 오류 **전의 1프레임은 처리**했는지(`frames == 1`, 차속 100 km/h) 확인
  - 소켓도 마찬가지: `read()` 실패를 일부러 내기 어렵다 (vcan0을 실행 중에 지우는 정도). 가짜 소스면 한 줄
  - 그 밖에 시각을 마음대로 정하는 것(`test_runs_bcm`: 1초 동안 100ms마다)도 가짜 소스가 쉽다. 실시간 소켓이면 정말로 1초를 기다려야 하고, 매번 몇 ms씩 흔들린다

## 4. 리팩터링 확인 (DoD: 기존 테스트 회귀 없음)

- 이번에 **바꾼 테스트**와 **안 바꾼 테스트**. 안 바꾼 테스트가 통과한다는 것의 의미 →

| | 파일 | `git diff --stat tests/` |
| --- | --- | --- |
| 바꾼 것 | `test_cli.c` (`iface_only` 기대값, 4줄 추가 2줄 삭제) | 이것 하나 |
| 새로 만든 것 | `test_frame_loop.c` (가짜 소스, 18개) | |
| 안 바꾼 것 | `test_log_reader.c`(45), `test_scenarios.c`(140), `test_bcm*.c`, `test_can_socket.c` ... 나머지 전부 | |

  - 의미: 파일 모드의 **겉 동작**(줄 수, 파싱 오류 수, 차량 상태, 전이 시각과 규칙, 0x400 106개)이 리팩터링 전과 **똑같다**는 증거. 속(`log_reader.c`를 셋으로 쪼갬)은 많이 바뀌었지만 밖에서 보면 그대로 → 이게 **리팩터링**의 정의 (동작은 그대로, 구조만 바꾸기)
  - 그래서 테스트를 **먼저 고치면 안 된다.** 기존 테스트를 같이 고치면 "원래대로 동작하는지"를 확인할 기준이 사라진다
  - M3 ~ M4에서 테스트를 많이 만들어 둔 덕분에 큰 구조 변경을 안심하고 할 수 있었다 (테스트 = 리팩터링 안전망)
- `test_cli.c`의 `iface_only`는 왜 기대값을 바꿨나 (회귀인가, 의도한 변경인가) → **의도한 변경.** M5-4에서는 `--iface`를 주면 `--dump`가 꼭 있어야 했다("아직 BCM을 못 돌림"이라는 임시 제한). M5-5에서 소켓으로 BCM을 돌릴 수 있게 됐으니 그 제한을 **일부러 없앴다**
  - 회귀 = 원래 되던 게 **실수로** 안 되는 것. 여기서는 안 되던 게 **일부러** 되게 바뀐 것(기능 추가) → 테스트도 새 동작에 맞춰 바꾸는 게 맞다
  - 구분법: 기대값을 바꿀 때 "이슈(요구사항)가 바뀌라고 했나?"를 묻는다. 그렇다면 의도한 변경, 아니라면 회귀(코드를 고쳐야 함)
- `log_read_stream()` / `LogReadStats`의 이름과 인자를 그대로 둔 이유 → 이 함수를 부르는 곳(`main.c`, `test_log_reader.c`, `test_scenarios.c`)을 **안 고치려고.** 이름이나 인자가 바뀌면 테스트도 고쳐야 하고, 그러면 위의 "안 바꾼 테스트로 확인"을 할 수 없다
  - 안쪽만 바꿨다: `log_read_stream` = `FileSource`를 만들어 `frame_loop_run`에 넘기는 **얇은 포장**. 바깥에서 보는 약속(시그니처)은 그대로
  - `LogReadStats`는 `typedef FrameLoopStats LogReadStats;`로 **같은 타입에 옛 이름**을 붙여 뒀다 (`LOG_MAX_TRANSITIONS`, `LOG_LINE_MAX`도 같은 방식)
  - Java로: 공개 API(메서드 시그니처)는 유지하고 구현만 새 클래스에 위임하는 것. 라이브러리가 버전을 올려도 쓰는 쪽 코드가 안 깨지게 하는 방법과 같다

## 5. 아직 안 되는 것 (→ M5-8)

`canplayer`가 끝난 뒤에도 터미널 A를 10초쯤 켜 두고 `wc -l rt.log`를 다시 본다.

| 확인 | 결과 |
| --- | --- |
| 재생 끝난 뒤 `rt.log` 줄 수가 늘었나 | ❌ 안 늘었다. 재생 직후 106 → 10초 뒤에도 **106** |
| 신호 끊김(`WARN 신호 끊김`)이 찍혔나 | ❌ 안 찍혔다 (0번). 마지막 로그는 10.577s `시동 IGN=OFF GEAR=P`에서 멈춤 |

- 왜 그런가 (`frame_loop_run`은 언제 `bcm_step`을 부르나) → `bcm_step`과 `bcm_tx_poll`을 **프레임을 받았을 때만** 부르기 때문이다. 루프는 `read_frame()`에서 프레임이 올 때까지 **멈춰서 기다린다**(blocking `read()`). 프레임이 안 오면 루프가 한 바퀴도 안 돈다 → BCM 판단도, 0x400 송신도 없다
  - BCM에게 시간은 "프레임이 올 때만" 흐른다. 10초 동안 아무것도 안 오면 BCM 입장에서는 시간이 **멈춘** 것 → 500ms 타임아웃(M4-6)도 판단할 기회가 없다
  - 파일 모드 시나리오 07(차속 끊김)에서는 3.600s에 `신호 끊김: 차속`이 잘 나왔다. 차속만 끊기고 0x300, 0x200은 계속 와서 그 프레임들이 `bcm_step`을 불러 줬기 때문. **모든** 신호가 끊기면(선이 빠지면) 아무도 깨워 주지 않아서 못 잡는다 → 가장 위험한 경우를 못 잡는 셈
  - 파일 모드는 끝이 있어서(END) 문제가 안 보였다. 실시간은 "끝"이 없어서 드러났다
- 진짜 BCM이라면 어떻게 돼야 하나 → 프레임과 상관없이 **정해진 주기(예: 10ms)마다** 판단해야 한다
  - 0x400은 프레임이 없어도 **100ms마다 계속** 나가야 한다 (다른 ECU가 "BCM이 살아 있다"를 Alive Counter로 확인하니까. 멈추면 BCM이 죽은 걸로 본다)
  - 아무 프레임도 안 오면 500ms 뒤 **신호 끊김 → WARNING(Fail-safe, T5)**으로 가야 한다
  - 방법(M5-8): `read()`에서 무한정 기다리지 않고 `poll()`로 "프레임이 오거나 **10ms가 지나면**" 깨어난다. 프레임이 오면 처리하고, 안 와도 `bcm_step(now)`와 `bcm_tx_poll(now)`를 부른다
  - 실제 ECU도 같다: 수신은 인터럽트로 받아 두고, 판단은 **주기 태스크**(10ms 태스크 등)가 돈다 (AUTOSAR의 주기 Runnable)

## 6. 정리

- 이번 이슈를 한 줄로 → 프레임을 **어디서 받는지**(파일/소켓/가짜)를 `CanSource` 인터페이스(함수 포인터 + `self`) 뒤로 숨겨서, **판단하는 루프 하나**(`frame_loop_run`)를 파일 모드와 실시간 모드가 같이 쓰게 했다. 기존 테스트는 안 바꾸고 통과
- `void *self` + 함수 포인터 방식이 실제로 쓰이는 곳 (예: 리눅스 커널, `qsort`) →
  - **`qsort(배열, 개수, 크기, compare)`**: 비교 함수를 함수 포인터로 넘긴다 (Java의 `Comparator`). 원소를 `const void *`로 받아서 안에서 형을 되돌린다 — 우리 `self`와 같은 방식
  - **리눅스 커널 `struct file_operations`**: `.read`, `.write`, `.open` 함수 포인터 묶음. 파일, 소켓, 장치(`/dev/...`)가 각자 채워 넣어서, 사용자는 어떤 것이든 똑같이 `read()`를 부른다. 우리가 `can_socket_read`에서 소켓을 `read()`로 읽을 수 있었던 이유 (M5-4)
  - **C 표준 `FILE *`**: `fgets`는 파일이든 키보드든 파이프든 같다. 안쪽에서 같은 방식으로 갈아 끼운다
  - **임베디드 드라이버 / AUTOSAR**: CAN 드라이버를 칩 회사마다 다르게 구현하고, 위층은 같은 함수(예: `Can_Write`)로 부른다. 하드웨어를 바꿔도 위층(우리로 치면 BCM)은 안 바뀐다
  - 콜백(`signal()`, 스레드 함수 `pthread_create(..., void *(*fn)(void *), void *arg)`)도 같은 "함수 포인터 + `void *` 데이터" 짝이다
