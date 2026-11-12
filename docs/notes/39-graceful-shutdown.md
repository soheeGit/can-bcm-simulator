# 39. 안전한 종료: 시그널 처리 (M5-9)

> 코드: `src/shutdown.c` (★ `shutdown_install()` 직접 채우기 1), `src/socket_source.c` (★ 종료 요청 확인 직접 채우기 2), `src/main.c`, `src/can_dump.c`, `tests/test_shutdown.c`, `tests/test_can_socket.c`
> 맥: `make test` → `[test_shutdown]` / VM: `make test` → `[test_can_socket]`, 실행 `./build-linux/bcm_sim -i vcan0`
> ⚠️ TODO 1을 먼저 채울 것. 안 채우면 VM의 `test_can_socket`도 SIGTERM에 죽어서 TODO 2를 확인할 수 없다

## 0. 직접 채우기

| 확인 | 결과 |
| --- | --- |
| 채우기 전 (맥 `./build/tests/test_shutdown; echo $?`) | 결과 줄 없이 **`Terminated: 15`**, 종료 코드 **143** / VM `make test`에서는 `test_shutdown`, `test_can_socket` 둘 다 `Terminated` |
| TODO 1만 채운 뒤 | ✅ 맥 `[test_shutdown]` `14 passed, 0 failed` / VM `[test_can_socket]` `103 passed, 2 failed` (`test_source_stops_on_request`: END 대신 TIMEOUT) |
| 둘 다 채운 뒤 | ✅ VM `[test_can_socket]` `105 passed, 0 failed`, 전체 `make test` 통과 (맥, VM) |

- 채우기 전에 테스트 프로그램이 "실패"가 아니라 "죽은" 이유. 종료 코드 143의 뜻 → 핸들러를 안 걸면 시그널마다 **기본 동작**이 실행되는데, SIGINT와 SIGTERM의 기본 동작은 "**프로세스 종료**"다. `raise(SIGTERM)`을 부른 순간 커널이 테스트 프로그램을 그 자리에서 끝냈다 → assert도, `TEST_SUMMARY`도 실행될 기회가 없음
  - 143 = **128 + 15**. 셸 규칙: 시그널로 죽으면 종료 코드가 128 + 시그널 번호. SIGTERM = 15 → 143, SIGINT(Ctrl+C) = 2 → 130, SIGKILL = 9 → 137
  - 출력이 하나도 안 보인 것도 같은 이유: `printf`는 버퍼에 모아 뒀다가 내보내는데(M5-4의 `fflush` 이야기), 갑자기 죽으면 버퍼를 비울 기회가 없어 사라진다
  - `make test`는 종료 코드가 0이 아니면 실패로 친다 → 셸이 `Terminated`를 찍고 `❌ 테스트 실패`
- 핸들러(`on_signal`)에서 `LOG_INFO("종료합니다")`나 `socket_source_close()`를 바로 하면 안 되는 이유 → 핸들러는 프로그램이 **어디를 실행하던 중이든** 끼어든다. 하던 일이 반쯤 된 상태에서 같은 걸 또 건드리면 망가진다
  - 예: 메인 흐름이 `LOG_INFO` → `fprintf` 도중(버퍼에 반쯤 씀, 내부 잠금을 잡은 상태)에 Ctrl+C → 핸들러가 또 `fprintf` → 같은 버퍼를 동시에 건드려 **글자가 섞이거나, 잠금을 기다리며 영원히 멈춘다**(deadlock)
  - 예: 메인이 `read(fd)` 도중에 핸들러가 `close(fd)` → 메인은 닫힌 fd로 계속 → 오류, 최악의 경우 그 번호를 새 파일이 받아서 엉뚱한 걸 읽음
  - 그래서 핸들러에서는 **async-signal-safe** 함수만 쓸 수 있다 (`write`, `_exit` 등 소수). `printf`, `malloc`, `fclose`는 안 된다
  - 가장 안전한 패턴 = "**표시만 세우고 바로 돌아가기**". 실제 일은 메인 흐름이 안전한 지점(루프 처음)에서 표시를 보고 한다
  - Java로: 인터럽트 받은 스레드에서 바로 자원을 닫지 않고 `Thread.interrupted()`를 루프에서 확인해서 빠져나오는 것과 같은 원칙

## 1. 흐름

```
Ctrl+C ──커널──→ on_signal(): g_stop_signal = 2   (표시만)
                     │
socket_source_read ──┤ poll이 EINTR로 깨어남 → continue → 바퀴 처음에서 shutdown_requested() → END
                     ↓
frame_loop_run: END → return 0
                     ↓
main: "종료 요청" 로그 → 소켓 닫기 → -o 파일 닫기 → 요약 출력 → 종료 코드 0
```

- `g_stop_signal`에 붙은 `volatile`과 `sig_atomic_t`가 각각 하는 일 →

| | 막는 문제 | 없으면 |
| --- | --- | --- |
| `volatile` | **컴파일러 최적화**: "이 변수는 이 함수 안에서 아무도 안 바꾸네?" → 값을 레지스터에 한 번 읽어 두고 다시 안 읽음 | 핸들러가 메모리의 값을 바꿔도 루프는 레지스터의 옛 값(0)만 보고 **영원히 안 멈춘다** (특히 `-O2` 빌드에서) |
| `sig_atomic_t` | **반쯤 쓰인 값**: 큰 타입은 쓰기가 여러 번에 나뉠 수 있다. 그 사이에 시그널이 끼어들면 반은 옛 값, 반은 새 값 | 읽는 쪽이 이상한 값을 볼 수 있다. `sig_atomic_t`는 "한 번에 읽고 쓰기"가 보장되는 정수 (보통 `int`) |

  - 둘 다 필요하다: `volatile`은 "**매번 다시 읽어라**", `sig_atomic_t`는 "**읽을 때 온전한 값이어라**"
  - 그래서 핸들러와 메인이 같이 쓰는 변수는 이것 **하나로만**(정수 표시 하나) 소통한다. 구조체나 문자열을 넘기려 하지 않는다
  - Java로: `volatile boolean running` — 다른 스레드가 바꾼 값을 보게 하는 같은 키워드. (단, 멀티스레드에서는 C의 `volatile`로 부족하고 `_Atomic`/잠금이 필요하다. 시그널 핸들러는 같은 스레드에 끼어드는 것이라 `volatile sig_atomic_t`로 충분)
- 핸들러에서 바로 `exit(0)`하지 않고 "END → 루프가 끝남 → main이 정리"로 돌아가는 이유 → 정리할 것이 **main에 있기** 때문이고, 핸들러에서는 정리를 안전하게 **할 수 없기** 때문이다
  - 정리할 것: 소켓(`ss`), `-o` 파일(`tx_out`), 통계(`stats`), 차량 상태(`in`), BCM(`bcm`)은 전부 **`main`의 지역 변수**다. 핸들러는 이것들을 모른다 (전역으로 빼야 함 → 설계가 나빠진다)
  - `exit()`은 async-signal-safe가 아니다 (0번: 버퍼 비우기, `atexit` 함수 실행 도중 메인과 부딪힐 수 있다). `_exit()`은 안전하지만 버퍼도 안 비우고 요약도 못 찍는다
  - "END"로 바꾸면 **파일 끝을 만났을 때와 같은 길**로 끝난다 → 정리 코드를 따로 만들 필요 없이, 이미 있는 정상 종료 경로(소켓 닫기 → 파일 닫기 → 요약)를 그대로 탄다
  - `frame_loop_run`은 시그널을 몰라도 된다 (소스가 END만 주면 끝남). 시그널을 아는 곳은 `shutdown.c`와 소스 한 군데뿐
  - Java/Spring으로: `@PreDestroy`나 `SmartLifecycle.stop()`처럼 "멈춰 신호 → 하던 요청 마무리 → 자원 닫기"를 프레임워크의 정상 경로로 하는 graceful shutdown과 같다

## 2. 끝내는 방법 비교 (VM, 터미널 2개)

터미널 A: `./build-linux/bcm_sim -i vcan0 -o rt.log` → 터미널 B: `canplayer -I data/scenarios/01_normal_drive.log vcan0=vcan0` 재생 도중에 아래 방법으로 끝낸다. 끝나면 터미널 A에서 `echo $?`

| 끝내는 방법 | 요약(결과/BCM)이 찍혔나 | `echo $?` |
| --- | --- | --- |
| 터미널 A에서 Ctrl+C (SIGINT) | ✅ 찍힘 (`종료 요청 (시그널 2)` → 결과 → `0x400 송신 : 24프레임` → `→ rt.log`) | ✅ **0** |
| 터미널 B에서 `kill $(pgrep -f "bcm_sim -i")` (SIGTERM) | ✅ 찍힘 (`종료 요청 (시그널 15)`) | ✅ **0** |
| 터미널 B에서 `kill -9 $(pgrep -f "bcm_sim -i")` (SIGKILL) | ❌ **안 찍힘.** 마지막 로그가 `차속 18.00 km/h`에서 뚝 끊기고 셸이 `Killed` | ❌ **137** (= 128 + 9) |

  - 셋 다 재생 2초쯤에 끝냈고, `rt.log`는 셋 다 24줄이었다 (`-o`는 줄마다 `fflush`해서(M5-5) 죽어도 이미 쓴 줄은 남는다)
- `kill -9`만 다르게 나오는 이유 → SIGKILL은 **잡을 수 없는** 시그널이다. 핸들러를 걸 수도, 막을 수도, 무시할 수도 없다 (`sigaction(SIGKILL, ...)`은 실패한다). 커널이 프로세스를 **그 자리에서** 없앤다 → 우리 코드는 한 줄도 더 실행되지 않는다
  - 일부러 이렇게 만들어져 있다: 프로그램이 핸들러에서 멈추거나 시그널을 무시해도 **관리자가 반드시 끝낼 수 있는** 마지막 수단. (SIGSTOP도 같다)
  - 그래서 운영 도구는 보통 "SIGTERM 먼저 → 몇 초 기다려도 안 끝나면 SIGKILL" 순서로 끈다 (`docker stop`은 10초, Kubernetes는 30초 유예 후 SIGKILL, `systemctl stop`도 같은 방식)
  - 우리 M5-10 스크립트도 SIGTERM(`kill`)으로 끄고 요약을 받아 본다
- (참고) 정리 중에 Ctrl+C를 한 번 더 누르면 바로 죽는다(`SA_RESETHAND`). 왜 이런 탈출구를 두나 → 정리하다가 **멈춰 버리면**(소켓 닫기가 안 끝남, 파일 쓰기가 막힘 등) 사용자가 끝낼 방법이 없어지기 때문
  - 핸들러가 계속 걸려 있으면 Ctrl+C를 아무리 눌러도 "표시만 세움" → 안 죽는다. 그러면 다른 터미널에서 `kill -9`를 찾아 쳐야 한다
  - `SA_RESETHAND`: 핸들러가 **한 번** 불린 뒤 기본 동작(종료)으로 돌아간다 → "첫 번째 Ctrl+C = 정중하게 끝내 줘, 두 번째 = 당장 끝내" (많은 서버 프로그램, `docker compose`도 이렇게 동작한다)

## 3. 어차피 커널이 다 닫아 주는데

프로세스가 죽으면 커널이 소켓, 파일을 전부 닫아 준다(M5-3 노트, fd).

- 그런데도 직접 정리하고 끝내는 이유 (우리 프로그램에서 실제로 달라지는 것) → 커널이 해 주는 건 "**자원 회수**"까지다. "**마무리**"는 프로그램만 할 수 있다

| | Ctrl+C로 그냥 죽을 때 (M5-8까지) | 안전한 종료 (M5-9) |
| --- | --- | --- |
| 소켓, 파일 fd | 커널이 닫아 줌 | 우리가 닫음 (결과는 같다) |
| 요약 (최종 상태, 전이 횟수, 0x400 수) | ❌ 없음 | ✅ 찍힘 |
| 종료 코드 | 130 (시그널로 죽음 = 실패처럼 보임) | 0 (정상) |
| 버퍼에 남은 출력 (`printf`, `-o` 파일) | 사라질 수 있다 (우리는 줄마다 `fflush`해서 피해 감) | 다 내보내고 닫는다 |
| 마지막 로그 | 아무 줄에서 뚝 끊김 → 정상 종료인지 사고인지 모름 | `종료 요청 (시그널 2)` → **의도한 종료**라는 기록 |

  - M5-10의 E2E 스크립트가 이걸 쓴다: `bcm_sim`을 `kill`로 끄고 → **요약을 읽어** 상태 전이가 기대와 맞는지 확인 → **종료 코드 0**인지 확인. 그냥 죽으면 둘 다 못 한다
  - 커널은 "열린 것을 닫는" 일은 해 주지만, "다 썼는지", "요약을 남겼는지", "다른 장치에 끝났다고 알렸는지"는 모른다. 그건 앱의 몫
  - 웹으로: 서버를 그냥 죽이면 커널이 TCP 연결은 끊어 주지만, 처리 중이던 요청은 응답 없이 끊기고 DB 트랜잭션은 롤백된다. graceful shutdown은 "새 요청은 그만 받고, 하던 건 끝내고, 커넥션 풀 닫고" 끝낸다
- 실제 ECU에서 "안전한 종료"는 무엇인가 → 시동을 끌 때(IGN OFF) 바로 전원을 끊지 않고 **정리 시간을 갖는 것**. 보통 "**슬립(sleep) 진입**" 절차라고 한다
  - 비휘발성 메모리(EEPROM/Flash)에 저장: 고장 코드(DTC), 학습값, 주행 거리, 설정 (쓰는 도중 전원이 끊기면 값이 깨진다 → 반드시 다 쓰고 끈다)
  - 출력을 **안전한 상태로**: 모터, 릴레이를 끄고, 잠금 같은 동작이 반쯤 된 상태로 남지 않게
  - 네트워크 관리(AUTOSAR NM): "나 이제 잔다"고 버스에 알리고, 다른 ECU들과 **같이** 잠든다. 혼자 갑자기 사라지면 다른 ECU가 "BCM 고장"(Alive Counter 멈춤, M4-8)으로 판단한다
  - 그래서 ECU 전원 회로에는 시동을 꺼도 잠깐 전원을 유지하는 장치가 있다 (ECU가 "다 끝났다"고 할 때까지)
  - 우리 M5-9는 그 작은 버전: 멈춰 신호 → 하던 판단 마무리 → 자원 닫기 → 결과 남기기. (차에서 SIGKILL에 해당하는 건 배터리 분리 — 그래서 ECU는 그것도 견디게 설계한다)

## 4. 정리

- 이번 이슈를 한 줄로 → Ctrl+C(SIGINT)와 `kill`(SIGTERM)을 받으면 핸들러는 **표시만** 세우고, 소켓 소스가 그걸 보고 "끝"(END)을 알려서, 파일 끝과 같은 **정상 종료 경로**(소켓 닫기 → 파일 닫기 → 요약 → 종료 코드 0)로 끝나게 했다. `kill -9`는 잡을 수 없다
- Java/Spring에서 같은 역할 →

| 우리 C | Java / Spring |
| --- | --- |
| `sigaction(SIGINT/SIGTERM, on_signal)` | `Runtime.getRuntime().addShutdownHook(thread)` (JVM이 SIGINT/SIGTERM을 받으면 훅 실행) |
| `volatile sig_atomic_t g_stop_signal` | `volatile boolean running` / `AtomicBoolean` |
| 소스가 END → 루프 종료 → main이 정리 | `@PreDestroy`, `DisposableBean.destroy()`, `SmartLifecycle.stop()` |
| 정리 순서: 받기 멈춤 → 마무리 → 소켓/파일 닫기 | Spring Boot `server.shutdown=graceful`: 새 요청 거절 → 처리 중인 요청 완료 대기 → 빈 정리 |
| `kill -9`는 못 잡음 | JVM도 `kill -9`에는 셧다운 훅이 **안 돈다** (같은 이유) |
| 두 번째 Ctrl+C = 강제 종료 | `spring.lifecycle.timeout-per-shutdown-phase` 넘으면 강제 진행 |
