# Linux 빌드/실행 환경 (SocketCAN은 Linux 전용이라 macOS에서는 이 컨테이너를 사용)
#
#   docker build -t can-dev .
#   docker run --rm -it --privileged -v "$PWD":/work can-dev bash
#
# --privileged : 컨테이너 안에서 vcan0 같은 네트워크 인터페이스를 만들기 위해 필요
FROM ubuntu:24.04

# build-essential : gcc, make 등 C 빌드 도구
# gdb             : 디버거
# can-utils       : candump, cansend, cangen, canplayer
# iproute2        : ip link (vcan0 생성)
# kmod            : modprobe, lsmod (커널 모듈 확인)
# file            : 실행 파일 형식 확인 (ELF / Mach-O)
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential gdb can-utils iproute2 kmod file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
