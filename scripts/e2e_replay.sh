#!/usr/bin/env bash
# E2E 테스트 (M5-10, VM 안에서 실행): 시나리오를 canplayer로 vcan0에 "진짜로" 재생하고,
# 실시간 bcm_sim의 판단이 파일 모드의 판단과 같은지 비교한다.
#
#   ./scripts/e2e_replay.sh                 # 04를 뺀 시나리오 전부
#   ./scripts/e2e_replay.sh 02 03           # 골라서
#   TOL_MS=300 ./scripts/e2e_replay.sh      # 시간 허용 오차 바꾸기 (기본 200ms)
#   make e2e                                # 빌드 + 이 스크립트
#
#  [기준]  bcm_sim -f 시나리오            → 상태 전이 (규칙, 시각)   ← 파일 모드 = 정답지 (오라클)
#  [실측]  bcm_sim -i vcan0 &  +  canplayer 시나리오  → kill(SIGTERM) → 상태 전이
#  [비교]  규칙 순서가 같은가 / 전이 사이 간격이 TOL_MS 안인가 / 정상 종료(0)했나 / 0x400이 나갔나
#
# 04(이상한 프레임)는 canplayer가 깨진 줄에서 멈추므로 뺀다 (32번 노트 3번)
set -uo pipefail

IF="${IFACE:-vcan0}"
BIN="${BIN:-./build-linux/bcm_sim}"
TOL_MS="${TOL_MS:-200}"
OUT_DIR="${OUT_DIR:-build-linux/e2e}"

die() { echo "❌ $*" >&2; exit 2; }

# ---------- 준비 확인 ----------
[ "$(uname -s)" = "Linux" ] || die "Linux(VM)에서 실행하세요: multipass shell can-vm"
[ -x "$BIN" ] || die "$BIN 이 없습니다. 먼저 make"
command -v canplayer >/dev/null || die "canplayer가 없습니다 (sudo apt install can-utils)"
ip link show "$IF" >/dev/null 2>&1 || die "$IF 가 없습니다. ./scripts/setup_vcan.sh"
if pgrep -x canplayer >/dev/null; then
    die "다른 canplayer가 돌고 있습니다. 끝나고 다시 (프레임이 섞인다)"
fi
mkdir -p "$OUT_DIR"

# ---------- 도우미 ----------

# 로그에서 상태 전이만 뽑기: "[   1.800s] INFO  STATE IDLE -> DRIVING_LOCKED (T3: ...)" → "T3 1800"
transitions() {
    grep ' STATE ' "$1" \
        | sed -E 's/^\[ *([0-9]+)\.([0-9]+)s\].*\((T[0-9]+|RESET):.*/\3 \1 \2/' \
        | awk '{ print $1, $2 * 1000 + $3 }'
}

# 요약에서 값 하나: "  최종 상태 : IDLE" → IDLE, "  0x400 송신 : 106프레임" → 106
summary_value() {   # $1 = 파일, $2 = 항목 이름
    grep -E "^  $2 *:" "$1" | head -1 | sed -E 's/.*: *([A-Z_0-9]+).*/\1/'
}

# 실시간 실행 한 번: bcm_sim을 띄우고 → 재생하고 → SIGTERM으로 끝낸다. 종료 코드를 출력
run_live() {    # $1 = 시나리오 파일, $2 = 로그 파일, $3 = 0x400 기록 파일
    "$BIN" -i "$IF" -o "$3" > "$2" 2>&1 &
    local pid=$!
    sleep 0.3                                   # 소켓을 열 시간 (먼저 켜 둬야 첫 프레임을 놓치지 않는다)

    canplayer -I "$1" "$IF=vcan0"               # 로그 속 vcan0 → 지금의 $IF 로. 로그 시각대로 재생 (몇 초 걸림)
    sleep 0.2                                   # 마지막 프레임 처리할 시간. 500ms(신호 끊김)보다는 짧게

    # bcm_sim을 "정중하게" 끝낸다 (M5-9: SIGTERM → 정리 → 요약 → 0). wait의 결과 = 그 프로세스의 종료 코드
    kill -TERM "$pid"
    wait "$pid"
    local code=$?
    kill -KILL "$pid" 2>/dev/null || true       # 안전장치: 그래도 살아 있으면 강제 종료 (다음 시나리오와 섞이지 않게)
    echo "$code"
}

# ---------- 시나리오 고르기 ----------
if [ $# -gt 0 ]; then
    SCENARIOS=()
    for n in "$@"; do
        SCENARIOS+=( data/scenarios/"$n"_*.log )
    done
else
    SCENARIOS=( data/scenarios/0[1-35-9]_*.log )   # 04 제외
fi

echo "E2E: ${#SCENARIOS[@]}개 시나리오를 $IF 로 재생 (시간 허용 오차 ${TOL_MS}ms, 결과: $OUT_DIR/)"
echo
# 열: 시나리오 / 전이 규칙 / 최대 시간 차 / 0x400 (실시간/파일) / 종료 코드 / 결과  (한글은 칸 폭이 어긋나서 영어로)
printf "%-30s %-14s %-9s %-9s %-5s %s\n" "scenario" "rules" "max_diff" "0x400" "exit" "result"
printf "%-30s %-14s %-9s %-9s %-5s %s\n" "--------" "-----" "--------" "-----" "----" "------"

fail=0
for scn in "${SCENARIOS[@]}"; do
    [ -f "$scn" ] || die "시나리오 파일이 없습니다: $scn"
    name="$(basename "$scn" .log)"
    file_log="$OUT_DIR/$name.file.log"
    live_log="$OUT_DIR/$name.live.log"
    live_tx="$OUT_DIR/$name.live.tx.log"

    # [기준] 파일 모드
    "$BIN" -f "$scn" > "$file_log" 2>&1
    mapfile -t exp < <(transitions "$file_log")

    # [실측] 실시간
    code="$(run_live "$scn" "$live_log" "$live_tx")"
    mapfile -t act < <(transitions "$live_log")

    problems=()

    # 1) 규칙 순서가 같은가 (T3 T6 T9 T7 ...)
    exp_rules="$(printf '%s\n' "${exp[@]}" | awk '{printf "%s ", $1}')"
    act_rules="$(printf '%s\n' "${act[@]}" | awk '{printf "%s ", $1}')"
    if [ "$exp_rules" != "$act_rules" ]; then
        problems+=("규칙이 다름: 기대 [${exp_rules% }] / 실제 [${act_rules% }]")
    fi

    # 2) 전이 사이 간격이 같은가 (시작 시각은 다르니, 첫 전이를 0으로 맞춰서 비교)
    max_diff=0
    if [ ${#exp[@]} -gt 0 ] && [ ${#exp[@]} -eq ${#act[@]} ]; then
        e0="$(echo "${exp[0]}" | awk '{print $2}')"
        a0="$(echo "${act[0]}" | awk '{print $2}')"
        for i in "${!exp[@]}"; do
            e="$(echo "${exp[$i]}" | awk '{print $2}')"
            a="$(echo "${act[$i]}" | awk '{print $2}')"
            d=$(( (a - a0) - (e - e0) ))
            d=${d#-}                            # 절댓값
            [ "$d" -gt "$max_diff" ] && max_diff=$d
        done
        if [ "$max_diff" -gt "$TOL_MS" ]; then
            problems+=("시간 차 ${max_diff}ms > 허용 ${TOL_MS}ms")
        fi
    fi

    # 3) 정상 종료했나 (SIGTERM → 요약 → 0, M5-9)
    [ "$code" = "0" ] || problems+=("종료 코드 $code (0이어야 함)")
    grep -q "최종 상태" "$live_log" || problems+=("요약이 없음")

    # 4) 최종 상태가 같은가
    exp_final="$(summary_value "$file_log" "최종 상태")"
    act_final="$(summary_value "$live_log" "최종 상태")"
    [ "$exp_final" = "$act_final" ] || problems+=("최종 상태: 기대 $exp_final / 실제 ${act_final:-없음}")

    # 5) 0x400이 나갔나 (파일 모드와 거의 같은 개수. 실시간은 시작/끝 몇 프레임 차이가 날 수 있다)
    exp_tx="$(summary_value "$file_log" "0x400 송신")"
    act_tx="$( [ -f "$live_tx" ] && wc -l < "$live_tx" || echo 0)"
    if [ "${act_tx:-0}" -lt $(( ${exp_tx:-0} - 3 )) ]; then
        problems+=("0x400이 적음: 기대 약 $exp_tx / 실제 $act_tx")
    fi

    if [ ${#problems[@]} -eq 0 ]; then
        result="✅ PASS"
    else
        result="❌ FAIL"
        fail=1
    fi
    printf "%-30s %-14s %-9s %-9s %-5s %s\n" \
        "$name" "${act_rules% }" "${max_diff}ms" "$act_tx/$exp_tx" "$code" "$result"
    for p in "${problems[@]}"; do
        echo "    - $p"
    done
done

echo
if [ $fail -eq 0 ]; then
    echo "✅ E2E 전부 통과"
else
    echo "❌ E2E 실패 (로그: $OUT_DIR/)"
fi
exit $fail
