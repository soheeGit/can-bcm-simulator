# 00. 개발 환경 구성 (M0-1)

## 1. macOS

- 칩: Apple Silicon (arm64)
- `gcc --version` 결과:

```
Apple clang version 21.0.0 (clang-2100.3.34.2)
Target: arm64-apple-darwin27.0.0
Thread model: posix
InstalledDir: /Library/Developer/CommandLineTools/usr/bin
```

- 알게 된 점: macOS의 `gcc`는 사실 `Apple clang` 이다.

## 2. Linux 컨테이너 (Docker)

- 베이스 이미지: `ubuntu:24.04`
- `docker info --format '{{.KernelVersion}}'` 결과: `6.10.14-linuxkit`
- 컨테이너 안 `gcc --version` 결과:

```
gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
Copyright (C) 2023 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```

## 3. 실행 파일 형식 비교

| 환경 | `file` 결과 |
| --- | --- |
| macOS (`playground/hello`) | `Mach-O 64-bit executable arm64` |
| Linux (`playground/hello_linux`) | `ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, BuildID[sha1]=676e6ecdcb9f9827d28096a2c58187c4eaa3ea3d, for GNU/Linux 3.7.0, not stripped` |

- 맥에서 만든 `hello`를 컨테이너에서 실행하면? → `cannot execute binary file: Exec format error` 발생
- 왜 그럴까? → CPU 아키텍처는 같다(arm64 = aarch64). 하지만 실행 파일 형식이 다르다(macOS = Mach-O, Linux = ELF). Linux는 ELF만 실행할 수 있어서 Mach-O 파일을 인식하지 못한다. → C는 실행할 OS에서 다시 컴파일해야 한다. (Java는 JVM 덕분에 한 번 빌드로 어디서든 실행)

## 4. vcan 동작 여부

```bash
ip link add dev vcan0 type vcan
ip link set up vcan0
ip -details link show vcan0
```

- 결과: ❌ 실패
- 출력 또는 에러 메시지:

```
Error: Unknown device type.
```

- 원인 확인:
```
  # CONFIG_CAN is not set
  modprobe: FATAL: Module vcan not found in directory /lib/modules/6.10.14-linuxkit
```
  → Docker Desktop 커널(linuxkit)은 CAN 기능 없이 빌드되어 있음

- `cansend` → `candump` 송수신 확인: vcan0 생성 실패로 확인 불가
- 결론 (M5에서 Docker를 쓸지 VM을 쓸지): M0~M4는 맥 + Docker로 충분 (vcan 불필요). M5에서 진짜 Ubuntu 커널이 도는 VM. Multipass 예정