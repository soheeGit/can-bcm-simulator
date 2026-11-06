# 31. Linux 환경에서 vcan0 만들기 (M5-1)

> 스크립트: `scripts/vm_setup.sh` (맥에서), `scripts/setup_vcan.sh` (VM 안에서)
> 실행: 맥에서 `./scripts/vm_setup.sh` → `multipass shell can-vm` → `cd ~/can-bcm-simulator`

## 1. 왜 VM인가

- Docker Desktop으로 안 되는 이유 (M0-1에서 확인한 것) → Docker Desktop이 쓰는 리눅스 커널이 **CAN 기능을 빼고** 만들어져 있다 (`# CONFIG_CAN is not set`). 커널에 없는 기능은 컨테이너 안에서 무엇을 설치해도 쓸 수 없다
  - 우리 VM은 `CONFIG_CAN=m`, `CONFIG_CAN_VCAN=m` → 모듈로 들어 있어서 `modprobe vcan`이 된다
- Docker(컨테이너)와 VM의 차이 (커널 기준으로) → 컨테이너는 **커널을 빌려 쓰고**, VM은 **자기 커널이 따로** 있다

| | Docker 컨테이너 | VM (Multipass) |
| --- | --- | --- |
| 커널 | 바깥(Docker Desktop의 리눅스)의 커널을 **같이 씀** | VM 안에 **자기 커널** |
| 커널 기능을 바꿀 수 있나 | 없음 (모듈을 올리려면 바깥 커널을 바꿔야 함) | 있음 (`modprobe vcan`, 모듈 설치) |
| 무게 | 가벼움, 빨리 켜짐 | 무거움 (메모리 2GB, 디스크 10GB) |
| 이 프로젝트 | M0~M4: 리눅스 빌드/테스트 확인 | **M5: vcan0이 필요한 SocketCAN** |

  - 비유: 컨테이너 = 한 건물 안의 **방**(전기, 수도는 건물 것을 같이 씀), VM = **따로 지은 집**(전기 설비를 마음대로 바꿀 수 있음). vcan은 "전기 설비(커널)"를 바꿔야 해서 집이 필요하다

## 2. 직접 해 보기

맥 터미널:

```bash
./scripts/vm_setup.sh
multipass shell can-vm
```

VM 안:

```bash
cd ~/can-bcm-simulator
ip -details link show vcan0
./scripts/setup_vcan.sh down && ip link show vcan0
./scripts/setup_vcan.sh up
```

| 확인 | 결과 |
| --- | --- |
| `ip -details link show vcan0` 첫 줄 | ✅ `4: vcan0: <NOARP,UP,LOWER_UP> mtu 72 qdisc noqueue state UNKNOWN ...` |
| `down` 후 `ip link show vcan0` | ✅ `vcan0 없음` (스크립트) / `Device "vcan0" does not exist.` (지워져서 없음) |
| `up`을 두 번 연속 실행하면 | 두 번 다 같은 `vcan0: <NOARP,UP,LOWER_UP> ...` 줄. 두 번째도 **에러 없음** (이미 있으면 만들기를 건너뜀) |

- 첫 줄 읽기

| 부분 | 뜻 |
| --- | --- |
| `4:` | 이 VM의 네트워크 장치 중 4번째 (번호는 만들 때마다 바뀔 수 있음) |
| `UP` | 켜져 있음 (`ip link set up`) |
| `LOWER_UP` | 실제 연결이 살아 있음. vcan은 선이 없으니 켜면 바로 |
| `NOARP` | 이더넷 주소 찾기(ARP)를 안 함. CAN에는 IP 주소가 없다 |
| `mtu 72` | 한 번에 보낼 수 있는 최대 크기(바이트). 일반 CAN 프레임은 16, 72는 CAN FD 프레임 크기 → vcan은 CAN FD까지 받을 수 있게 만들어짐 |
| `state UNKNOWN` | 가상 장치라 "연결 상태"가 따로 없음. 정상 |

## 3. vcan0을 만드는 세 단계

```bash
sudo modprobe vcan                   # ①
sudo ip link add dev vcan0 type vcan # ②
sudo ip link set up vcan0            # ③
```

- ①, ②, ③이 각각 하는 일 →

| | 명령 | 하는 일 | 비유 (랜카드) |
| --- | --- | --- | --- |
| ① | `modprobe vcan` | vcan **드라이버**(커널 모듈)를 커널에 올림 | 드라이버 설치 |
| ② | `ip link add dev vcan0 type vcan` | vcan 종류의 장치를 `vcan0`이라는 이름으로 **만듦** | 랜카드 꽂기 |
| ③ | `ip link set up vcan0` | 장치를 **켬** (꺼져 있으면 프레임이 안 오감) | 랜 연결 켜기 |
- `sudo`가 필요한 이유 → 세 명령 모두 **커널**(드라이버 올리기)이나 **네트워크 장치**를 바꾸는 일이라, 일반 사용자가 하면 시스템 전체에 영향을 줄 수 있어서 관리자만 허용된다
  - 반대로 이미 만들어진 `vcan0`에서 `candump`, `cansend`로 주고받는 건 `sudo` 없이 된다 (M5 프로그램도 일반 사용자로 실행)
- 웹 개발과 비교 (vcan0을 무엇에 비유할 수 있나) → **`lo`(localhost)** 같은 **가상 네트워크 장치**. 밖으로 안 나가고 내 컴퓨터 안에서만 오간다
  - `eth0`(진짜 랜카드) ↔ `can0`(진짜 CAN 하드웨어), `lo`(가상) ↔ `vcan0`(가상)
  - 웹 서버를 `localhost:8080`에 띄워 놓고 `curl localhost:8080`으로 시험하듯, BCM을 `vcan0`에 붙여 놓고 `cansend vcan0 ...`로 시험한다
  - "API 통신"은 그 위에서 오가는 **내용**(요청/응답)에 가깝고, vcan0은 그 내용이 지나가는 **길(장치)**이다. CAN 프레임(0x100 등)이 API 메시지 쪽

## 4. 설치하다 막혔던 것

- `modprobe: FATAL: Module vcan not found`가 난 이유와 해결 (`CONFIG_CAN_VCAN=m`인데도) → 커널은 vcan을 **지원하지만**, 그 모듈 **파일**(`vcan.ko`)이 설치돼 있지 않았다
  - `=m`은 "커널에 넣지 않고 **따로 모듈 파일로** 만들었다"는 뜻. Ubuntu는 덜 쓰는 모듈 파일을 `linux-modules-extra` 패키지로 빼 두고, 클라우드 이미지(Multipass)에는 기본으로 안 깐다
  - 해결: `sudo apt install linux-modules-extra-$(uname -r)` → `modprobe vcan` 성공. `$(uname -r)`은 "지금 쓰는 커널 버전"(6.8.0-147-generic). 커널 버전과 모듈 버전이 같아야 해서 이렇게 쓴다
  - 주의: 나중에 커널이 업데이트되면(VM 재시작 후 `uname -r`이 바뀜) 새 버전의 extra를 다시 깔아야 한다 → `setup_vcan.sh`가 실패하면 설치 명령을 알려 준다
- 프로젝트 폴더를 연결(`multipass mount`)했는데 VM에서 비어 보인 이유와 해결 → 맥의 **개인정보 보호**(TCC)가 Multipass의 백그라운드 프로그램(`multipassd`)이 `~/Documents`를 읽지 못하게 막았다. 연결은 됐지만 맥 쪽에서 파일 목록을 못 줘서 빈 폴더로 보였다
  - 해결: 시스템 설정 → 개인정보 보호 및 보안 → **전체 디스크 접근 권한**에 `multipassd` 추가 → `sudo launchctl kickstart -k system/com.canonical.multipassd`로 재시작 (실행 중인 프로그램은 바뀐 권한을 모르니까)
  - 에러 메시지 없이 **조용히 비어 보이는** 게 함정. `vm_setup.sh`는 연결 후 `Makefile`이 보이는지 확인하고, 안 보이면 이 해결 방법을 알려 준다

## 5. 스크립트

- `setup_vcan.sh up`을 두 번 실행해도 에러가 안 나게 만든 부분 → **"이미 있으면 건너뛰기"** 확인을 각 단계 앞에 뒀다

```bash
if ! lsmod | grep -q '^vcan'; then sudo modprobe vcan; fi                         # 드라이버가 없을 때만
if ! ip link show "$IF" >/dev/null 2>&1; then sudo ip link add dev "$IF" type vcan; fi   # 장치가 없을 때만
sudo ip link set up "$IF"                                                          # 켜기는 이미 켜져 있어도 에러 안 남
```

  - 확인 없이 `ip link add`를 두 번 하면 `RTNETLINK answers: File exists` 에러가 난다
  - 이렇게 **몇 번 실행해도 결과가 같은** 성질을 **멱등성**(idempotent)이라고 한다. HTTP의 `PUT`이 멱등(같은 요청을 여러 번 보내도 결과가 같음)인 것과 같은 말
  - `down`도 같다: 없으면 지우지 않고 넘어간다
  - `set -euo pipefail`: 중간에 명령 하나라도 실패하면 **바로 멈춘다** → 반쯤 된 상태로 "성공"처럼 끝나지 않게
- 이슈의 완료 조건 "한 줄 명령으로 다시 만들 수 있다"를 어떻게 만족하나 → 상황별로 **한 줄**씩

| 상황 | 한 줄 | 어디서 |
| --- | --- | --- |
| 처음부터 (VM이 없을 때, 새 맥에서) | `./scripts/vm_setup.sh` → VM 만들기, 패키지, 폴더 연결, vcan0까지 | 맥 |
| VM을 다시 켰을 때 (vcan0은 재시작하면 사라짐) | `./scripts/setup_vcan.sh` | VM 안 |

  - `vm_setup.sh`도 멱등이다: VM이 이미 있으면 만들기를 건너뛰고, 이미 연결돼 있으면 다시 연결하지 않는다 → 고장 났을 때 그냥 다시 돌리면 된다
  - 과정을 스크립트로 남기면 "어떻게 만들었더라?"를 기억할 필요가 없다. 다른 사람(면접관)도 같은 환경을 만들 수 있다 (Infrastructure as Code의 작은 버전)
