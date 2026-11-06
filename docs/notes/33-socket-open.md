# 33. SocketCAN 소켓 열기 & 바인드 (M5-3)

> 코드: `include/can_socket.h`, `src/can_socket.c` (★ 3단계 bind 직접 채울 곳), `tests/test_can_socket.c`, `tools/can_socket_demo.c`, `Makefile`
> 실행: **VM 안에서** `make test`, `make socket_demo && ./build/can_socket_demo vcan0`
> 맥에서는 이 코드가 빌드에서 빠진다 (SocketCAN이 없어서)

## 0. 직접 채우기: bind

`src/can_socket.c`의 `TODO(직접 채우기)`. 주석의 ① ② ③대로.

| 확인 (VM 안 `make test`의 `[test_can_socket]`) | 결과 |
| --- | --- |
| 채우기 전 | `14 passed, 1 failed` — `FAIL tests/test_can_socket.c:35: addr.can_ifindex` 기대 7, 실제 0. 소켓은 열렸지만(fd ≥ 0) **아무 장치에도 안 묶여서** 장치 번호가 0 |
| 채운 뒤 | ✅ `15 passed, 0 failed`, 전체 `make test` 통과 (VM, 엄격 경고도 통과). `getsockname`이 vcan0의 번호(7)를 돌려줌 |

- 맥에서 `make test`를 돌리면 `[test_can_socket]`이 **아예 없다** (Makefile이 리눅스 전용 코드를 뺌) → 소켓 테스트는 **VM에서** 돌려야 한다. 처음에 맥에서 돌려서 "다 통과"로 보였던 이유

- 막혔던 곳 → 코드는 **한 번에** 맞게 썼다. 실패 처리도 위의 ioctl과 같은 순서(errno 보관 → `perror` → `close` → errno 되돌림 → `return -1`)로 넣었다
  - 헷갈렸던 것: 맥에서 테스트가 통과로 보인 것(위), 맥 VS Code의 빨간 줄(`linux/can.h`는 리눅스에만 있어서 맥에서 못 찾음. 실제 빌드는 VM에서 하니 무시)

## 1. 데모 실행

터미널 A: `make socket_demo && ./build/can_socket_demo vcan0` (Enter를 누르지 말고 기다린다)
터미널 B: 화면에 나온 명령 `ls -l /proc/<pid>/fd`

| 확인 | 결과 |
| --- | --- |
| 터미널 A의 `fd = ?` | ✅ 3 |
| 터미널 B의 `ls -l /proc/<pid>/fd` (0, 1, 2와 fd 번호 줄) | ✅ `0 -> /dev/pts/1` / `1 -> /dev/pts/1` / `2 -> /dev/pts/1` / `3 -> 'socket:[30348]'` |
| `./build/can_socket_demo nope0` | ✅ `ioctl(SIOCGIFINDEX): No such device` / `열기 실패: nope0` (종료 코드 1) |

- 읽는 법: `/dev/pts/1` = 지금 쓰고 있는 **터미널 창**. 0(입력), 1(출력), 2(에러)가 전부 같은 터미널에 연결돼 있다. `socket:[30348]`의 숫자는 커널이 소켓에 붙인 번호
- `nope0`: 2단계(ioctl, 이름 → 장치 번호)에서 "그런 장치 없음"으로 실패 → 소켓을 닫고 -1. 이슈 완료 조건 "없는 인터페이스 이름에서 명확한 에러" ✅
- fd가 3번인 이유 → 프로그램이 시작할 때 **0, 1, 2번은 이미 열려 있다** (stdin, stdout, stderr). 새로 여는 것은 **비어 있는 가장 작은 번호**를 받으므로 3번
  - 하나 더 열면 4번 (`test_open_vcan0`에서 두 번 열어 번호가 다른 걸 확인). 닫으면 그 번호가 비어서 다음에 다시 쓰인다
  - Java의 `System.in` / `System.out` / `System.err`가 0 / 1 / 2번에 해당

## 2. 소켓을 여는 세 단계

```c
int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);      /* 1) */
ioctl(fd, SIOCGIFINDEX, &ifr);                   /* 2) */
bind(fd, (struct sockaddr *)&addr, sizeof(addr)); /* 3) */
```

| 단계 | CAN | TCP (Java의 Socket 안에서 일어나는 일) |
| --- | --- | --- |
| 1) 소켓 만들기 | `socket(PF_CAN, SOCK_RAW, CAN_RAW)` → **CAN용** 통신 창구 하나를 받는다 (fd 3) | `socket(AF_INET, SOCK_STREAM, 0)` → 인터넷(TCP)용 창구 |
| 2) 상대 찾기 | `ioctl(SIOCGIFINDEX)` → 이름 `"vcan0"`을 커널의 **장치 번호**(7)로 바꾼다 | DNS: 이름 `"example.com"`을 **IP 주소**로 바꾼다 |
| 3) 연결하기 | `bind(장치 번호)` → 이 창구는 **vcan0**에서 오가는 프레임을 쓴다 | `connect(IP:포트)` → 이 창구는 **그 서버**와 통신한다 |

- 한 줄로: **창구 만들기 → 이름을 번호로 → 그 번호에 연결**
- Java에서는 `new Socket("example.com", 80)` **한 줄**이 안에서 1) ~ 3)을 다 해 준다. C는 하나씩 직접 부른다
- `socket()`의 세 값
  - `PF_CAN`: 주소 종류 = CAN (TCP는 `AF_INET` = 인터넷)
  - `SOCK_RAW`: 프레임을 **그대로** 주고받음 (TCP의 `SOCK_STREAM`은 바이트 흐름)
  - `CAN_RAW`: CAN의 기본 방식 (다른 방식으로 주기 송신용 `CAN_BCM` 등이 있음)
- 다른 점: TCP는 상대 **한 명**과 1:1로 연결되지만, CAN은 버스에 묶이면 **그 버스의 모든 프레임**을 듣는다 (방송, 32번 노트)

## 3. 파일 디스크립터(fd)

- fd가 무엇인가 → 운영체제가 "이 프로그램이 열어 둔 것"에 붙여 주는 **번호표**(정수). 프로그램은 그 번호만 들고 있다가 `read(3, ...)`, `close(3)`처럼 번호로 부탁한다
  - 실제 소켓, 파일의 정보는 **커널 안**에 있고, 프로그램은 번호만 안다. 식당 진동벨 번호 / 코트 보관표와 같다
  - Java로 치면 `Socket`, `FileInputStream` 객체 안에 숨어 있는 번호. Java는 객체로 감싸서 직접 볼 일이 없을 뿐
- 소켓인데 왜 "파일" 디스크립터인가 (`read`, `write`, `close`) → 리눅스(유닉스)는 **"모든 것은 파일"**처럼 다룬다. 파일, 터미널(`/dev/pts/1`), 소켓, 파이프가 전부 같은 번호 목록(1번 실험의 `/proc/<pid>/fd`)에 들어가고, **같은 함수**(`read`, `write`, `close`)로 쓴다
  - 그래서 M5-4에서 CAN 프레임을 받을 때도 파일 읽듯 `read(fd, &frame, sizeof(frame))`를 쓴다
- `close()`를 안 하면 → 프로그램이 끝날 때까지 번호와 커널 자원이 **계속 잡혀 있다** (자원 누수). 한 프로그램이 열 수 있는 개수에는 한도가 있어서(보통 1024개, `ulimit -n`), 계속 열고 안 닫으면 결국 `Too many open files`로 더 못 연다
  - 프로그램이 끝나면 운영체제가 대신 닫아 주긴 한다. 하지만 BCM처럼 **계속 돌아가는** 프로그램은 끝나지 않으니 직접 닫아야 한다
  - 그래서 `can_socket_open()`도 2), 3)단계에서 실패하면 1)에서 연 소켓을 **닫고** 돌아간다
  - Java의 `try-with-resources`로 `close()`를 빠뜨리지 않게 하는 것과 같은 문제 (M3-3의 `fclose`와 `goto cleanup`도 같은 이야기)

## 4. 에러 처리

- `perror("ioctl(SIOCGIFINDEX)")`가 찍는 것 → **"내가 준 글자: errno의 뜻"**을 stderr에 찍는다. 1번의 `ioctl(SIOCGIFINDEX): No such device`가 바로 이것
  - 시스템 함수(`socket`, `ioctl`, `bind`)는 실패하면 `-1`만 돌려주고, **왜** 실패했는지는 전역 변수 `errno`에 번호로 남긴다 (`ENODEV` = 19 = No such device)
  - `perror`는 그 번호를 사람이 읽는 문장으로 바꿔 준다. 앞에 붙이는 글자로 "**어느 단계**에서" 실패했는지 알 수 있게 했다 (M3-3의 `strerror(errno)`와 같은 정보)
- 실패했을 때 `errno`를 `saved`에 보관했다가 되돌리는 이유 → `perror()`, `close()`도 함수라서 **안에서 errno를 바꿀 수 있기** 때문. 그대로 두면 부른 쪽이 `errno`를 볼 때 원래 원인(`ENODEV`)이 아니라 엉뚱한 값을 볼 수 있다

```c
int saved = errno;      /* ① 원래 원인을 사진 찍어 두고 */
perror("...");          /* ② 이 안에서 errno가 바뀔 수도 */
close(fd);              /* ③ 여기서도 */
errno = saved;          /* ④ 원래 원인으로 되돌림 */
return -1;
```

  - 헤더에 "실패하면 errno에 원인이 남는다"고 약속했고, 테스트가 `ASSERT_EQ(ENODEV, errno)`로 이 약속을 확인한다
  - Java로 치면 예외를 잡아서 정리(close)하다가 원래 예외를 잃어버리지 않게 다시 던지는 것
- 실패해도 `close(fd)`를 하는 이유 → 1)에서 소켓은 **이미 열렸기** 때문. 2), 3)이 실패해서 `-1`을 돌려주면 부른 쪽은 fd 번호를 모르니 **아무도 닫을 수 없다** → 열린 채로 새는 소켓(누수)이 된다 (3번 질문)
  - 규칙: "**연 사람이 닫는다.** 성공해서 넘겨줄 때만 부른 쪽이 닫는다(`can_socket_close`)"

## 5. Linux 전용 빌드

- Makefile의 `UNAME_S := $(shell uname -s)`와 `filter-out`이 하는 일 → **지금 컴퓨터가 리눅스가 아니면, 리눅스 전용 파일을 빌드 목록에서 뺀다**

```makefile
UNAME_S := $(shell uname -s)                 # 운영체제 이름: 맥은 Darwin, 리눅스는 Linux
SRCS := $(wildcard src/*.c)                  # src의 .c 전부
ifneq ($(UNAME_S),Linux)                     # 리눅스가 아니면
SRCS := $(filter-out src/can_socket.c,$(SRCS))   # 그 목록에서 can_socket.c를 뺀다
endif
```

  - 테스트(`test_can_socket.c`)도 같은 방식으로 뺀다
  - 안 빼면 맥에서 `make`할 때 `linux/can.h`를 못 찾아서 **빌드 전체가 실패**한다 → 맥에서 M0 ~ M4 작업도 못 하게 된다
  - 그래서 맥에서 `make test`를 돌리면 `[test_can_socket]`이 아예 안 나온다 (0번)
- `can_socket.c` 맨 위의 `#define _DEFAULT_SOURCE`가 필요한 이유 → 우리는 `-std=c11`(**순수 C 표준**만)로 컴파일한다. `struct ifreq`, `IFNAMSIZ`는 C 표준이 아니라 **리눅스/BSD 확장**이라, 이 모드에서는 헤더가 일부러 숨긴다. `_DEFAULT_SOURCE`는 "확장 기능도 보여 줘"라는 스위치
  - 빼고 컴파일해 보면 (VM에서 확인): `error: 'IFNAMSIZ' undeclared`, `error: storage size of 'ifr' isn't known`
  - **include보다 먼저** 써야 한다. 헤더가 읽힐 때 이 값을 보고 무엇을 보여 줄지 정하기 때문
  - 이 파일에만 둔 이유: 다른 파일은 순수 C로 충분하다. 확장이 필요한 곳(리눅스 전용 파일)에만 범위를 좁혔다

## 6. 테스트

- `getsockname()`으로 묶인 장치 번호를 확인하게 한 이유 (0번의 TODO 상태와 연결) → **"소켓이 열렸다"만으로는 bind가 됐는지 모르기** 때문. bind를 빼도 1)단계에서 소켓은 열려서 `fd >= 0`은 통과한다
  - `getsockname()` = 소켓에게 "**너 어디에 묶여 있니?**"를 묻는 함수. bind 전이면 장치 번호 0, bind 후면 vcan0의 번호(7)
  - 실제로 TODO 상태에서 `fd >= 0`은 통과하고 이 확인만 `기대 7, 실제 0`으로 실패했다 → 이 줄이 없었다면 bind를 안 채워도 "다 통과"였을 것
  - 27, 28번 노트와 같은 교훈: "결과가 나왔다"(fd)가 아니라 "**하려던 일을 했나**"(묶였나)를 확인해야 한다
- vcan0이 없으면 건너뛰게 한 이유 → 테스트 결과가 **코드가 아니라 환경**(vcan0을 만들었나)에 따라 실패하지 않게 하려고. VM을 다시 켜면 vcan0이 사라지는데, 그때 "코드가 틀렸다"처럼 보이면 헷갈린다
  - 건너뛸 때는 `(vcan0 없음: 건너뜀. ./scripts/setup_vcan.sh)`를 찍어서 **조용히 넘어가지 않게** 했다 (건너뛴 줄 모르고 "통과"로 믿으면 안 되니까)
  - vcan0이 **없어도** 되는 테스트(`nope0`, 잘못된 이름)는 항상 돈다
  - JUnit의 `Assumptions.assumeTrue(...)`(조건이 안 맞으면 실패가 아니라 건너뜀)와 같다
  - 이슈 완료 조건 확인: "vcan0 바인드 성공" = `test_open_vcan0`, "없는 인터페이스 이름에서 명확한 에러" = `test_open_unknown_iface` ✅
