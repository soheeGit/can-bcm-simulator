# 36. 0x400 BCM_STATUS 송신 (write) (M5-6)

> 코드: `src/can_socket.c` (★ `can_frame_to_linux()` 직접 채우기 1, `can_socket_write()`의 `write` 줄 직접 채우기 2), `include/can_sink.h`, `src/file_sink.c`, `src/socket_sink.c`, `src/frame_loop.c`, `src/main.c`, `tests/test_can_socket.c`, `tests/test_frame_loop.c`
> VM: `make test` → `[test_can_socket]` / 실행 `./build-linux/bcm_sim -i vcan0`

## 0. 직접 채우기

| 확인 (VM `make test`의 `[test_can_socket]`) | 결과 |
| --- | --- |
| 채우기 전 | `42 passed, 16 failed` — `test_to_linux_standard` (15), `test_write_to_vcan0` (1) |
| TODO 1만 채운 뒤 | `57 passed, 1 failed` — `test_to_linux_*` 통과, `test_write_to_vcan0`의 `w`만 실패 (`n = -1`이라 항상 송신 실패) |
| 채운 뒤 | ✅ `62 passed, 0 failed`, 전체 `make test` 통과 (VM, 엄격 경고도 통과) |

- 막혔던 곳 ①: **`in->can_id`** → 빌드 오류 `'CanFrame' has no member named 'can_id'`
  - 이번 방향은 **우리 → 리눅스**라 `in`이 우리 `CanFrame`(`id`, `dlc`, `data`), `out`이 리눅스 `struct can_frame`(`can_id`, `len`, `data`). M5-4(`from_linux`)와 `in`/`out`의 타입이 **뒤바뀌었다**
  - 같은 뜻인데 이름이 다른 칸: `id` ↔ `can_id`, `dlc` ↔ `len`. 변환 함수는 결국 "이름이 다른 칸끼리 옮겨 적기"
- 막혔던 곳 ②: `memcpy();` 인자 없음 → `memcpy(받을 곳, 줄 곳, 바이트 수)` = `memcpy(out->data, in->data, in->dlc)`
- 막혔던 곳 ③: TODO 2의 `write` → `write(어디로, 무엇을, 몇 바이트)` = `write(fd, &raw, sizeof(raw))`. `read`와 인자 순서가 같고 방향만 반대 (read: 소켓 → `raw`, write: `raw` → 소켓)
- `& CAN_SFF_MASK`는 정리 때 뺐다: 바로 위에서 0x7FF 넘는 ID를 이미 거르고, 우리 `CanFrame`에는 표시(flag) 비트가 없다. (`from_linux`에서는 리눅스 `can_id`에 표시 비트가 섞여 있어서 필요했다)
- 채우기 전에도 `test_to_linux_rejects`가 통과한 이유 → 이 테스트는 "ID 0x800, DLC 9, NULL이면 **`false`**"만 본다. 비어 있는 함수는 **무엇이든 `false`**라서 우연히 통과
  - 짝인 `test_to_linux_standard`("정상 프레임은 **`true`**")가 실패해서 비어 있다는 걸 잡았다 (34번 노트 0번과 같은 "짝 테스트")
- `test_write_to_vcan0`에서 `if (w == 0)`으로 `read()`를 감싼 이유 (없으면 채우기 전에 무슨 일이 생기나) → **테스트가 영원히 멈춘다**
  - `read()`는 프레임이 **올 때까지 기다린다**(blocking, M5-4). 송신이 실패하면 아무것도 안 오니 끝없이 기다린다 → `make test`가 안 끝남 (처음엔 이걸 빠뜨려서 VM 테스트가 3분 넘게 멈췄다)
  - 테스트는 실패하더라도 **"실패"라고 보고하고 끝나야** 한다. 멈추면 무엇이 왜 실패했는지도 알 수 없다
  - 그래서 "보냈으면(`w == 0`) 받아 본다" → `ASSERT_EQ(0, w)` 실패 1개로 깔끔하게 끝남. 받기 확인 4개가 이때 건너뛰어져서, 채우기 전후로 assert 수가 58 → 62로 달라진다
  - 더 튼튼하게 하려면 기다리는 시간에 제한을 둔다 (`SO_RCVTIMEO` 소켓 옵션이나 `poll()`, M5-8)

## 1. 구조: 출력도 인터페이스로

```
                         ┌─ SocketSink ──write()──→ vcan0      (M5-6)
frame_loop ──0x400──→ CanSink[] ┤
                         └─ FileSink  ──fputs()──→ rt.log     (M4-8 --out)
                         (테스트) ArraySink → 배열
```

- `CanSource`(M5-5)와 비교해서 `CanSink`가 다른 점 하나 (`frame_loop_run`이 받는 모양) → 소스는 **하나**(`const CanSource *src`), 출력은 **여러 개**(`const CanSink *sinks, size_t sink_count` = 배열 + 개수)
  - 입력은 한 곳에서만 받는다 (파일 **또는** 소켓). 출력은 동시에 여러 곳: `-i vcan0 -o rt.log`면 **버스로 송신 + 파일에 기록** 두 곳
  - 루프는 `for (i = 0; i < sink_count; i++) sinks[i].write_frame(...)`로 **몇 개든, 무엇이든** 같은 방식으로 부른다. 0개(`NULL, 0`)면 만들기만 하고 안 내보냄 (`test_log_reader`처럼)
  - Java로: `List<CanSink> sinks`를 돌면서 `sink.writeFrame(f)`. C 배열은 자기 길이를 모르니 개수를 같이 넘긴다
  - 한 곳이 실패해도 다음 곳에는 보낸다 (`test_sink_failure_keeps_running`)
- 받는 소켓과 보내는 소켓을 따로 안 열고 같은 `fd`를 쓴다. 왜 되나 → 소켓은 **양방향**이다. fd 하나로 `read()`(받기)도 `write()`(보내기)도 된다
  - TCP 연결 하나로 요청을 보내고 응답을 받는 것과 같다 (전화기 하나로 듣고 말하기)
  - 그래서 `SocketSink`는 소켓을 **열지 않고** `SocketSource`의 `ss.fd`를 빌려 쓴다. 닫는 것도 연 쪽(`socket_source_close`) 한 곳만 → 두 번 닫는 실수가 없다
  - 따로 열어도 동작은 한다 (테스트의 `rx`, `tx`처럼). 다만 fd가 하나 더 들고, 3번의 "자기 프레임을 다시 받나"가 달라진다

## 2. 송신 확인 (VM, 터미널 3개)

| 터미널 A | 터미널 B | 터미널 C |
| --- | --- | --- |
| `./build-linux/bcm_sim -i vcan0` | `candump -td vcan0,400:7FF` | `canplayer -I data/scenarios/02_door_open_while_driving.log vcan0=vcan0` |

| 확인 | 결과 |
| --- | --- |
| 터미널 B에 나온 0x400 개수 (끝나고 Ctrl+C, `-td`는 앞 줄과의 시간 차) | ✅ **106개** (파일 모드 `0x400 송신 : 106프레임`, M5-5의 `rt.log` 106줄과 같다) |
| 간격(`-td`)은 대략 얼마인가 | ✅ **약 0.1초** (0.0991 ~ 0.1006). 100ms 송신 주기(`BCM_TX_PERIOD_MS`, M4-8)대로 |
| 잠금 명령(`data[1]` bit 0)이 1인 줄과 그때 `data[0]`(상태) | ✅ `400 [8] 01 01 00 00 00 00 00 03` **딱 한 줄**. `data[0]=01`(DRIVING_LOCKED), `data[1]=01`(잠금 명령), `data[7]=03`(alive). 터미널 A의 `STATE IDLE -> DRIVING_LOCKED (T3)`, `도어 잠금 명령`과 같은 때 |
| `data[7]`(Alive Counter)이 15 다음에 | ✅ `... 0F` 다음 줄이 `... 00` → **0으로 돌아간다** (0 ~ 15를 반복, `% 16`) |

- 잠금 명령이 한 줄뿐인 이유: Mealy 출력(T3 전이 **순간**에만, M4-8의 `lock_cmd_pending`)이라 다음 송신 한 번에만 실리고 지워진다. 바로 다음 줄은 `01 00 ... 04`(상태는 잠금 그대로, 명령은 0)
- `candump`의 `-td`가 정확히 0.100000이 아니고 0.0992, 0.1003처럼 흔들리는 이유 → BCM이 **자기 타이머로** 보내는 게 아니라, **받은 프레임에 맞춰서** 보내기 때문
  - 0x400은 프레임을 받을 때 `bcm_tx_poll(now)`이 "100ms 지났나?"를 보고 나간다. 그 프레임은 `canplayer`가 보내는데, `canplayer`도 OS 위의 프로그램이라 0.1초를 **정확히** 못 기다린다 (스케줄링, 깨어나는 데 걸리는 시간)
  - 그 흔들림 + 우리 프로그램이 받아서 처리하는 시간이 그대로 0x400 간격에 실린다
  - 평균은 0.1초 근처로 맞는다: "다음 송신 시각"을 `지금 + 100`이 아니라 `예정 시각 + 100`으로 정해서 (M4-8의 `next_tx_ms`) 오차가 **쌓이지 않는다**
  - 실제 ECU는 하드웨어 타이머 인터럽트로 보내서 흔들림이 수 µs 수준. 우리는 리눅스 일반 프로세스라 ms 단위로 흔들린다 (실시간 OS가 따로 있는 이유)

> 실험 중 실수로 `bcm_sim`이 **두 개** 떠 있었더니 0x400이 **212개**, 같은 프레임이 0.00001초 차이로 두 번씩 찍혔다 (`... 0F` / `... 0F`). 한 버스에 같은 ID를 보내는 노드가 둘 = 실제 차에서는 **설계 오류**다 (누구 말을 믿어야 할지 모름, 실제 CAN은 같은 ID가 동시에 나가면 중재도 깨진다). 하나만 켜면 106개

## 3. 자기 프레임은 다시 받나

터미널 A를 `-l debug`로 다시 띄우고, 터미널 C에서 `cansend vcan0 300#0203`를 한 번 보낸다.

| 확인 | 결과 |
| --- | --- |
| 터미널 A의 DEBUG에 `RX 0x400`이 찍혔나 | ❌ **안 찍혔다.** `RX 0x300 [2] 02 03 → OK` 다음에 `TX 0x400 [8] 00 ... 00`만 있고, 그 0x400을 다시 받은 `RX 0x400`은 없다 |
| 터미널 B(`candump`)에는 0x400이 보이나 | ✅ **보인다.** `vcan0 300 [2] 02 03` 다음 줄에 `vcan0 400 [8] 00 00 00 00 00 00 00 00` |

- 왜 그런가 (SocketCAN의 loopback, `CAN_RAW_RECV_OWN_MSGS`) → 리눅스가 보낸 프레임을 **같은 컴퓨터의 다른 소켓들에게는 돌려주고**(loopback), **보낸 그 소켓에게는 안 돌려준다**(`CAN_RAW_RECV_OWN_MSGS` 기본값 0)
  - loopback(기본 켜짐): 실제 CAN 버스에서는 모든 노드가 버스의 모든 프레임을 듣는다. 같은 컴퓨터 안의 다른 프로그램(`candump`)도 그렇게 보이게 커널이 복사해 준다 → 그래서 `candump`가 우리 0x400을 본다
  - `RECV_OWN_MSGS`(기본 꺼짐): 내가 보낸 걸 나한테까지 돌려주면 대부분 쓸데없다 (내가 뭘 보냈는지는 이미 안다) → 그래서 BCM 소켓은 자기 0x400을 안 받는다
  - 1번의 "fd를 따로 열면 달라진다": 받는 소켓과 보내는 소켓이 **다르면**, 받는 소켓 입장에서는 "다른 소켓이 보낸 것" → 자기 0x400을 **받는다**. 테스트 `test_write_to_vcan0`이 `tx`로 보내고 `rx`로 받을 수 있는 것도 이 때문
  - 받더라도 지금은 문제없다: 디스패처가 0x400을 모르는 ID로 보고 버린다(`IGNORED`). 하지만 쓸데없이 처리하는 프레임이 늘어난다 → M5-7에서 **수신 필터**(0x100, 0x200, 0x300만)로 커널 단계에서 막는다
  - Java로: 이벤트 버스에 발행하면 다른 구독자는 다 받지만, 발행자는 자기 이벤트를 다시 안 받게 설정된 것

## 4. 송신이 실패하면

- `write()`가 실패할 수 있는 경우 (실제 CAN 버스에서) →

| 상황 | `errno` | 무슨 일 |
| --- | --- | --- |
| 송신 큐가 꽉 참 | `ENOBUFS` | 버스에 **ACK 해 줄 노드가 없음**(나 혼자 연결), 버스가 너무 바빠 내 차례가 안 옴 → 못 나간 프레임이 커널 큐에 쌓이다 넘친다. 가장 흔한 경우 |
| 장치가 꺼짐 | `ENETDOWN` | `ip link set can0 down`, 케이블/트랜시버 문제로 인터페이스가 내려감 |
| Bus-off | `ENETDOWN` 등 | 송신 오류가 계속 쌓이면 CAN 컨트롤러가 **스스로 버스에서 빠진다**(고장 난 노드가 버스 전체를 망치지 않게, M1 오류 카운터). 재시작 전까지 못 보냄 |
| 프레임이 이상함 | `EINVAL` | 우리 `can_frame_to_linux`가 먼저 막는다 (ID > 0x7FF, DLC > 8) |

  - vcan은 가상이라 ACK도 버스 경합도 없어서 거의 실패하지 않는다. 실패는 진짜 버스(`can0`)에서 주로 생긴다
- 송신이 실패해도 `frame_loop_run`은 멈추지 않고 `tx_errors`만 센다. 왜 멈추면 안 되나 (`test_sink_failure_keeps_running`) → **송신은 BCM이 하는 일의 일부일 뿐**이다. 0x400 한 번 못 보냈다고 판단(잠금, 경고)까지 멈추면 더 위험하다
  - 송신 실패는 대개 **잠깐**이다 (큐가 비면, 버스가 한가해지면 다시 된다). 멈추면 회복될 기회가 없다
  - BCM이 멈추면 다음 판단도, 다음 0x400도 없다 → 다른 ECU는 Alive Counter가 멈춘 걸 보고 "BCM 죽음"으로 판단한다. **한 번 실패 → 전체 고장**으로 키우는 꼴
  - 그래서 "실패는 **기록**(WARN 로그 + `tx_errors`)하고, 할 수 있는 일은 **계속**"한다. 파일 출력이 실패해도 버스 송신은 계속 (테스트: 나쁜 출력 10회 실패, 좋은 출력 10개 정상, 상태도 DRIVING_LOCKED 그대로)
  - M3에서 깨진 줄을 만나도 WARN 찍고 다음 줄로 넘어간 것과 같은 원칙 (graceful degradation: 일부가 고장 나도 나머지는 동작)
  - 다만 계속 실패한다면(bus-off) 실제 ECU는 진단 코드(DTC)를 저장하고 복구 절차를 밟는다. 우리는 횟수만 센다 (요약의 `송신 실패 : N회`)

## 5. 정리

- `cansend`가 하던 일을 우리 프로그램이 한다. 이제 BCM 프로그램 = ? → **`candump`(받기, M5-4) + BCM 판단(M4) + `cansend`(보내기, M5-6)를 한 프로그램에.** 32번 노트 5번에서 "M5에서 만들 것"이라고 한 모양이 완성됐다
  - 받기: `SocketSource` → 판단: `frame_loop_run`(디스패처 → `bcm_step`) → 보내기: `SocketSink`
  - 입력과 출력이 둘 다 인터페이스(`CanSource`, `CanSink`)라서, 같은 판단 코드가 파일 모드 / 실시간 모드 / 테스트(가짜)에서 그대로 돈다
  - 이제 진짜 ECU처럼 **버스에 붙은 노드 하나**다. 다른 프로그램(`candump`, 나중에 계기판 역할)이 우리 0x400을 듣고 쓸 수 있다
- M5-5 노트 5번의 문제(프레임이 안 오면 0x400도 안 나감)는 해결됐나 → **아니다, 그대로다.** 송신 **방법**(소켓으로 `write`)만 생겼고, 송신 **시점**은 여전히 "프레임을 받았을 때"뿐이다
  - `canplayer`가 끝나면 0x400도 멈추고, 모든 신호가 끊겨도 신호 끊김을 판단하지 못한다
  - 2번에서 0x400 간격이 흔들린 것도 같은 원인 (받은 프레임에 맞춰 보내서)
  - M5-8에서 `poll()`로 "프레임이 오거나 10ms가 지나면" 깨어나 주기적으로 `bcm_step`과 `bcm_tx_poll`을 부르면 해결된다
