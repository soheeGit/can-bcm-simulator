#!/usr/bin/env bash
# 맥에서 실행: Multipass로 Ubuntu VM(can-vm)을 만들고 CAN 실습 환경을 준비한다 (M5-1)
#
#   ./scripts/vm_setup.sh            # VM 만들기 + 패키지 설치 + 프로젝트 폴더 연결 + vcan0
#   multipass shell can-vm           # VM 안으로 들어가기
#   cd ~/can-bcm-simulator           # (VM 안) 맥의 프로젝트 폴더가 그대로 보인다
#
# Docker Desktop의 Linux 커널은 CAN이 꺼져 있어서(CONFIG_CAN is not set) vcan을 못 만든다 → 진짜 VM이 필요
set -euo pipefail

VM="${VM:-can-vm}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
VM_DIR="/home/ubuntu/can-bcm-simulator"

if ! command -v multipass >/dev/null 2>&1; then
    echo "multipass가 없습니다: brew install --cask multipass" >&2
    exit 1
fi

# 1) VM 만들기 (이미 있으면 건너뜀)
if ! multipass info "$VM" >/dev/null 2>&1; then
    multipass launch 24.04 --name "$VM" --cpus 2 --memory 2G --disk 10G
fi

# 2) 패키지: vcan 모듈(Ubuntu 클라우드 이미지에는 빠져 있음), can-utils, 컴파일러
multipass exec "$VM" -- bash -c '
    sudo apt-get update -qq
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
        linux-modules-extra-$(uname -r) can-utils build-essential gdb
'

# 3) 맥의 프로젝트 폴더를 VM에 연결 (맥에서 고치면 VM에서 바로 보인다)
if ! multipass info "$VM" | grep -q "$VM_DIR"; then
    multipass mount "$PROJECT_DIR" "$VM:$VM_DIR"
fi
if ! multipass exec "$VM" -- test -f "$VM_DIR/Makefile"; then
    echo "프로젝트 폴더가 VM에서 비어 보입니다." >&2
    echo "맥 시스템 설정 → 개인정보 보호 및 보안 → 전체 디스크 접근 권한에 multipassd를 추가하세요." >&2
    echo "  (/Library/Application Support/com.canonical.multipass/bin/multipassd)" >&2
    exit 1
fi

# 4) vcan0
multipass exec "$VM" -- bash "$VM_DIR/scripts/setup_vcan.sh" up

echo
echo "준비 완료. 들어가기: multipass shell $VM"
