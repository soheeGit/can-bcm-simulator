# 34. 프레임 수신(read) & CanFrame 변환 (M5-4)

> 코드: `src/can_socket.c` (★ `can_frame_from_linux()` 직접 채울 곳), `src/can_dump.c`, `src/signals.c`(`can_frame_describe`), `src/cli.c`, `src/main.c`, `src/logger.c`, `Makefile`
> 실행 (VM 안): `make && ./build-linux/bcm_sim --iface vcan0 --dump`
> ⚠️ 이제 VM의 빌드 폴더는 `build-linux/`다 (맥은 그대로 `build/`)

## 0. 직접 채우기: `can_frame_from_linux()`

| 확인 (VM 안 `make test`의 `[test_can_socket]`) | 결과 |
| --- | --- |
| 채우기 전 | `25 passed, 10 failed` — `test_from_linux_standard` (7), `test_read_from_vcan0` (3). 함수가 항상 `false`라서 어떤 프레임도 "우리 프레임"으로 안 바뀜 → vcan0에서 받은 0x100도 `CAN_READ_SKIP`(무시)이 됨 |
| 채운 뒤 | ✅ `35 passed, 0 failed`, 전체 `make test` 통과 (VM, 엄격 경고도 통과). 덤프 모드에서 `0x100`, `0x101`, `0x300`은 받고 29비트 ID, Remote는 무시 |

- 채우기 전에도 통과한 `test_from_linux_ignored`는 왜 통과했나 → 이 테스트는 "EFF / RTR / ERR / 길이 9는 **`false`**여야 한다"만 본다. 비어 있는 함수는 **무엇이든** `false`라서 우연히 통과
  - 짝인 `test_from_linux_standard`("일반 프레임은 **`true`**여야 한다")가 실패해서 비어 있다는 걸 잡았다 (27, 28번 노트와 같은 "짝 테스트")
- 막혔던 곳 → **연산자 우선순위.** 처음 쓴 코드가 빌드되지 않았다

```c
if (in->can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG) != 0u)      /* 처음 */
if ((in->can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) != 0u)    /* 고친 것: 괄호 한 쌍 */
```

  - C에서는 `!=`가 `&`보다 **먼저** 계산된다 → 처음 코드는 `in->can_id & (마스크 != 0u)` = `in->can_id & 1` → "ID가 **홀수**인가?"를 검사하는 코드였다. 0x101 같은 정상 프레임을 버리고, 짝수인 29비트 ID는 통과시켰을 것
  - gcc가 `suggest parentheses around comparison in operand of '&'`(`-Wall`의 `-Wparentheses`)로 잡아 줬고, `-Werror`라서 빌드가 멈췄다 → 경고 옵션 덕분에 찾기 어려운 버그를 미리 막음
  - 규칙: `&`, `|`를 비교(`==`, `!=`)와 섞을 때는 **항상 괄호**. 덤프 모드에서 `0x101`(홀수 ID)을 보내 제대로 받는 것도 확인
  - 나머지(길이 검사, `CAN_SFF_MASK`, `memset`, `memcpy`)는 처음부터 맞았다

## 1. 덤프 모드 실행

터미널 A: `./build-linux/bcm_sim --iface vcan0 --dump`
터미널 B: 아래를 하나씩

| 터미널 B | 터미널 A에 찍히는 것 |
| --- | --- |
| `cansend vcan0 100#1027000000000000` | `RX 0x100 [8] 10 27 00 ... → 차속 100.00 km/h` |
| `cansend vcan0 300#0203` | `RX 0x300 [2] 02 03 → 시동 IGN=ON GEAR=D` |
| `cansend vcan0 7FF#DEADBEEF` | `RX 0x7FF [4] DE AD BE EF → -` (받았지만 우리가 모르는 ID) |
| `cansend vcan0 100#10` | `RX 0x100 [1] 10 → 해석 불가` (차속은 2바이트가 필요한데 1바이트) |
| `cansend vcan0 12345678#00` | **아무것도 안 찍힘** (29비트 ID → `CAN_READ_SKIP`. `-l debug`로 실행하면 `무시: Extended / Remote / 에러 프레임`) |
| `cansend vcan0 100#R` | **아무것도 안 찍힘** (Remote 프레임 → 무시) |
| `canplayer -I data/scenarios/02_door_open_while_driving.log vcan0=vcan0` | 10초 동안 `RX` **318줄**이 실시간으로 올라온다 (0x300 → 0x200 → 0x100 순서, 4.0초쯤 `도어 FL=1`) |

- 결과가 네 가지로 나뉜다: **해석됨** / **모르는 ID**(`-`) / **해석 불가**(형식은 맞는데 내용 부족) / **무시**(우리가 안 다루는 종류, 안 찍힘)
- `candump vcan0`과 비교해서 다른 점 → `candump`는 **바이트만** 보여 준다(`100 [8] 10 27 ...`). 우리 덤프는 신호 정의서대로 **뜻까지** 풀어 준다(`차속 100.00 km/h`). 대신 `candump`는 29비트 ID, Remote 프레임도 다 보여 주고, 우리는 우리가 다루는 일반 프레임만 보여 준다
  - 실제 차 개발에서 쓰는 도구(CANoe 등)도 DBC 파일을 넣으면 이렇게 "뜻"으로 보여 준다. 우리 `can_frame_describe()`가 그 역할의 작은 버전

## 2. Linux `struct can_frame` → 우리 `CanFrame`

```c
struct can_frame {          /* <linux/can.h> */
    canid_t can_id;         /* 32비트: ID + 표시(flag) 3비트 */
    __u8    len;            /* 데이터 길이 */
    __u8    __pad, __res0, __res1;
    __u8    data[8];
};
```

- 왜 그대로 쓰지 않고 우리 `CanFrame`으로 바꾸나 → 디스패처, 디코더, BCM(M2 ~ M4)은 전부 **`CanFrame`**을 받는다. 리눅스 형식은 **리눅스 전용**이라 그걸 쓰면 맥에서 빌드도 테스트도 안 된다
  - 바꾸는 곳을 **입구 한 군데**(`can_frame_from_linux`)로 모으면, 나머지 코드는 프레임이 파일에서 왔는지 소켓에서 왔는지 몰라도 된다 (M5-5로 이어짐)
  - 웹으로 치면 외부 API 응답(외부 DTO)을 받자마자 우리 도메인 객체로 바꾸는 것 (안티 커럽션 레이어). 바깥 형식이 바뀌어도 이 함수만 고치면 된다
- `can_id`의 맨 위 3비트(EFF / RTR / ERR)가 뜻하는 것과, 무시하는 이유 → `can_id`는 32비트인데 ID는 아래쪽만 쓰고, **맨 위 3비트는 "이 프레임이 어떤 종류인지" 표시**다

| 비트 | 이름 | 뜻 | 무시하는 이유 |
| --- | --- | --- | --- |
| 31 | `CAN_EFF_FLAG` | Extended: ID가 29비트 | 우리 정의서는 11비트(Standard) ID만 쓴다 (M3-2 파서도 8자리 ID는 UNSUPPORTED) |
| 30 | `CAN_RTR_FLAG` | Remote: "이 ID 데이터를 보내 줘"라는 **요청**, 데이터 없음 | 우리는 데이터 프레임만 해석한다 |
| 29 | `CAN_ERR_FLAG` | 에러 프레임 (버스 오류 알림) | 신호가 아니다 |

  - 표시가 켜진 걸 그냥 ID로 읽으면 엉뚱한 ID가 된다. 예: `12345678#00`의 `can_id`는 `0x92345678`(EFF 켜짐) → 아래 11비트만 보면 `0x678`이라는 **가짜 ID**
- `CAN_SFF_MASK`(0x7FF)로 `&` 하는 이유 → `can_id`에서 **아래 11비트(진짜 ID)만** 남기려고. SFF = Standard Frame Format, `0x7FF` = 11비트가 전부 1인 마스크 (M0, M2의 `&` 마스크와 같은 원리, 32번 노트 4번의 필터 마스크와도 같다)
  - 앞에서 표시 3비트가 꺼진 것을 확인했으니 사실상 같은 값이지만, "ID는 11비트"라는 뜻을 코드에 분명히 남긴다

## 3. `read()`

- `read(fd, &raw, sizeof(raw))`가 하는 일, 돌려주는 값 → "fd에서 최대 `sizeof(raw)`바이트를 읽어서 `raw`에 넣어라". 파일을 읽을 때와 **같은 함수**다 (33번 노트 3번: 소켓도 파일처럼)
  - 돌려주는 값: **실제로 읽은 바이트 수**(0 이상) / 실패하면 `-1`(원인은 `errno`)
  - CAN_RAW 소켓은 한 번의 `read()`에 **프레임 하나**를 통째로 준다 → 정상이면 항상 16
- 받은 바이트 수가 `sizeof(struct can_frame)`(16)이 아니면 에러로 본 이유 → 덜 받았는데 그대로 쓰면 `raw`의 나머지 칸은 **쓰레기 값**이라 엉뚱한 ID, 데이터로 해석된다 → 읽은 양을 꼭 확인하고, 다르면 쓰지 않는다
  - 예: 소켓이 CAN FD 모드로 열려 있으면 72바이트짜리 프레임(`struct canfd_frame`)이 온다. 그런 "예상과 다른 크기"를 그냥 넘기지 않으려는 것
  - 웹으로 치면 HTTP 응답의 `Content-Length`와 실제로 받은 길이가 다르면 버리는 것
- 프레임이 안 오면 `read()`는 어떻게 되나 (blocking) → 프레임이 올 때까지 **그 줄에서 멈춰서 기다린다**(블로킹). 그래서 덤프 모드는 아무것도 안 보내면 가만히 있고, 보내는 순간 한 줄 찍는다
  - 문제: 기다리는 동안 **다른 일을 못 한다.** BCM은 프레임이 안 와도 "500ms 끊김"을 판단해야 하는데(M4-6), `read()`에서 멈춰 있으면 판단할 기회가 없다 → M5-8에서 `poll()`로 "최대 10ms만 기다리고 돌아오기"를 해서 해결
  - Java로 치면 `InputStream.read()`, `BufferedReader.readLine()`이 블로킹이다. 그래서 서버는 스레드를 따로 두거나 NIO(`Selector`)를 쓴다 → C에서는 `poll()`/`select()`가 NIO의 `Selector` 역할

## 4. 만들다 생긴 문제 두 가지

- 맥에서 `ld: unknown file type in 'build/bit_utils.o'`가 난 이유와 해결 (`build-linux/`) → 맥과 VM이 **같은 `build/` 폴더를 공유**해서, VM에서 만든 **리눅스용 `.o`**를 맥의 링커가 읽으려다 "모르는 파일 형식"이라고 실패했다. 맥(arm64 Mach-O)과 리눅스(arm64 ELF)는 같은 CPU여도 **파일 형식이 다르다**
  - 맥의 make는 `.o`가 `.c`보다 새것이라 "이미 최신"으로 보고 다시 컴파일하지 않았다 (make는 파일 시각만 보지, 누가 만들었는지는 모른다)
  - 해결: Makefile에서 운영체제마다 빌드 폴더를 나눴다 → 맥은 `build/`, 리눅스는 `build-linux/` (`.gitignore`에도 추가). 그래서 VM에서는 `./build-linux/bcm_sim`
- 덤프 출력이 비어 있던 이유와 해결 (`fflush`, M5-2의 `candump -l`과 같은 이유) → 출력을 바로 내보내지 않고 **모아 뒀다가(버퍼) 꽉 차면** 한꺼번에 내보낸다. 덤프를 `timeout`으로 강제 종료하자 모아 둔 줄이 **내보내지기 전에 버려졌다**
  - 화면(터미널)에 바로 찍을 때는 줄마다 내보내지만, 파일이나 파이프로 보내면(`> /tmp/dump.txt`) 4KB쯤 모일 때까지 기다린다 → 프레임이 많던 시나리오 재생은 일부만 나오고, `cansend` 8개는 하나도 안 나왔다
  - 해결: 로거(`log_write`)가 한 줄 찍을 때마다 `fflush(out)` → 실시간 모드에서는 **받는 즉시** 보여야 하니까
  - 대가: 매 줄 내보내서 조금 느려진다. 로그 몇 줄 정도는 상관없다

## 5. 테스트

- 결과 → `[test_can_socket]` **35 passed** (VM). 전체 `make test` 통과 (맥에서는 `test_can_socket`이 빠지고 나머지 통과)
  - 추가된 것: `test_from_linux_standard` / `test_from_linux_ignored` (변환), `test_read_from_vcan0` (진짜 소켓으로 보내고 받기), `test_describe` (`test_signals`, 맥에서도 돎), `test_iface_option` (`test_cli`, 맥에서도 돎)
- `test_read_from_vcan0`이 소켓 두 개(tx, rx)를 쓰는 이유 → **소켓은 자기가 보낸 프레임을 기본으로 안 받는다.** 그래서 보내는 소켓(tx)과 받는 소켓(rx)을 따로 연다. 터미널 B의 `cansend`(tx)와 터미널 A의 덤프(rx)를 **한 테스트 안에서** 흉내 낸 것
  - 순서: tx로 `write()` → rx로 `can_socket_read()` → 받은 값 확인. 29비트 ID도 보내서 `CAN_READ_SKIP`이 나오는지 확인
  - 그 밖의 변환 테스트(`test_from_linux_*`)는 소켓 없이 `struct can_frame`을 직접 만들어서 확인 → vcan0 없이도 돈다
