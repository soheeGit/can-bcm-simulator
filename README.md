# can-bcm-simulator
[미니 프로젝트] 가상 CAN 패킷 기반 차량 상태 제어 시뮬레이터 (C언어)

## 빌드 & 실행

```bash
make              # 빌드 → build/bcm_sim
make test         # 단위 테스트 실행 (실패 시 종료 코드 0이 아님)
make debug        # AddressSanitizer + UBSan 포함 빌드 → build/debug/bcm_sim
make clean
```

학습 로드맵: [docs/ROADMAP.md](docs/ROADMAP.md)
