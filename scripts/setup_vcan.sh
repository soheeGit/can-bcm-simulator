#!/usr/bin/env bash
# VM(Linux) 안에서 실행: 가상 CAN 장치 vcan0 만들기 / 지우기 / 확인 (M5-1)
#
#   ./scripts/setup_vcan.sh          # 만들기 (= up). 이미 있으면 그대로 둔다
#   ./scripts/setup_vcan.sh down     # 지우기
#   ./scripts/setup_vcan.sh status   # 확인
#   IFACE=vcan1 ./scripts/setup_vcan.sh   # 다른 이름으로
set -euo pipefail

IF="${IFACE:-vcan0}"

usage() {
    echo "사용법: $0 [up|down|status]" >&2
}

case "${1:-up}" in
    up)
        # 1) 커널 모듈 올리기 (vcan 드라이버)
        if ! lsmod | grep -q '^vcan'; then
            if ! sudo modprobe vcan; then
                echo "vcan 모듈이 없습니다. 설치: sudo apt install linux-modules-extra-$(uname -r)" >&2
                exit 1
            fi
        fi
        # 2) 장치 만들기 (이미 있으면 건너뜀)
        if ! ip link show "$IF" >/dev/null 2>&1; then
            sudo ip link add dev "$IF" type vcan
        fi
        # 3) 켜기 (eth0을 up 하는 것과 같다)
        sudo ip link set up "$IF"
        ip -details link show "$IF"
        ;;
    down)
        if ip link show "$IF" >/dev/null 2>&1; then
            sudo ip link delete "$IF"
        fi
        echo "$IF 없음"
        ;;
    status)
        ip -details link show "$IF"
        ;;
    *)
        usage
        exit 2
        ;;
esac
