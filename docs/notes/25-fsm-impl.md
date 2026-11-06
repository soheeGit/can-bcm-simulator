# 25. switch-case 기반 FSM 구현 (M4-3)

> 코드: `src/bcm.c` (★ 직접 채울 곳), `include/bcm.h`, `src/log_reader.c`, `src/main.c`, `tests/test_bcm.c`, `Makefile`
> 실행: `make && make test`, `./build/bcm_sim -f data/scenarios/02_door_open_while_driving.log`

## 0. 직접 채우기: DRIVING_LOCKED의 T5 ~ T7

`src/bcm.c`의 `bcm_step` 안에서 `TODO(직접 채우기)`를 찾는다 (`Cmd + F`).

1. **채우기 전에** 실행해 보고 결과를 적는다

```bash
make && make test
./build/bcm_sim -f data/scenarios/02_door_open_while_driving.log | grep STATE
```

| 확인 | 결과 |
| --- | --- |
| `make test`에서 실패한 테스트 이름 | `[test_bcm]` 78 passed, **13 failed**: `test_t5_fail_safe` (6), `test_t6_door_open_while_driving` (3), `test_t7_parked` (3), `test_priority` (1). 괄호는 그 테스트 안에서 실패한 `ASSERT` 개수 |
| 시나리오 02의 `STATE` 로그 (몇 줄, 무엇) | ✅ **2줄**: `[1.800s] STATE IDLE -> DRIVING_LOCKED (T3: 시동 ON, 15km/h 이상, 문 닫힘)` / `[10.000s] STATE DRIVING_LOCKED -> IDLE (T1: 시동 OFF/ACC)` |

2. 바로 위 `case BCM_STATE_IDLE:`와 아래 `case BCM_STATE_WARNING:`를 보고 같은 모양으로 T5, T6, T7을 쓴다
3. `make test`가 전부 통과할 때까지 고친다. 막히면 실패 메시지의 **줄 번호**로 `tests/test_bcm.c`를 열어서 어떤 상황인지 본다
4. **채운 뒤에** 다시 실행해서 적는다

| 확인 | 결과 |
| --- | --- |
| `make test` | ✅ `[test_bcm]` **91 passed, 0 failed**, 모든 테스트 통과 (`make clean` 후 처음부터 빌드해서 확인) |
| 시나리오 02의 `STATE` 로그 | ✅ **4줄**: `[1.800s] IDLE -> DRIVING_LOCKED (T3)` / `[4.000s] DRIVING_LOCKED -> WARNING (T6)` / `[5.000s] WARNING -> DRIVING_LOCKED (T9)` / `[9.500s] DRIVING_LOCKED -> IDLE (T7)` → `fsm-design.md` 9번, README 예시와 같음 |

- 채우기 전에는 시나리오 02에서 4.0초에 아무 일도 없고 10.0초에야 IDLE이 된 이유 → DRIVING_LOCKED의 `case`가 비어 있어서 **어떤 입력이 와도 맞는 줄이 없었기 때문.** `next = prev`, `rule = NONE` 그대로라 상태가 안 바뀐다. 그래서 1.8초에 잠금 상태에 들어간 뒤로는:
  - 4.0초 문 열림 → T6이 없으니 WARNING으로 못 감 (그래서 5.0초 T9도 없음)
  - 9.5초 정차 + P → T7이 없으니 IDLE로 못 감
  - 빠져나갈 길은 `switch` **밖**에 있는 **T1**(시동 OFF) 하나뿐 → 10.0초에 시동을 끌 때 IDLE
  - T1을 `switch` 밖 맨 앞에 둔 덕분에, 다른 규칙이 비어 있어도 시동을 끄면 반드시 IDLE로 돌아온다 (2번 질문과 연결)
- 막혔던 곳 / 처음에 틀렸던 것 → T5, T6에서 `rule`만 정하고 **`next = BCM_STATE_WARNING;`을 빠뜨렸다** (T7에는 있었음). 그래서 4개가 실패했다

| 실패 | 이유 |
| --- | --- |
| 175, 205줄 `out.state` | `next`를 안 바꿔서 `prev`(DRIVING_LOCKED) 그대로 |
| 177, 207줄 `out.warning_buzzer` | 경고음은 `state == WARNING`일 때만 켜지는데(Moore) 상태가 안 바뀌어서 같이 꺼짐 |

  - `rule`을 확인하는 176, 206줄은 통과했다 → "규칙은 맞게 찾았는데 상태를 안 옮겼다"는 걸 테스트가 정확히 짚어 줬다
  - 한 줄을 고치니 두 개(상태, 경고음)가 같이 통과했다 → 경고음이 상태에서 **계산되는**(Moore) 출력이라서
  - 교훈: T2, T8처럼 "규칙은 맞았지만 상태는 유지"인 줄도 있어서, `rule`만 정하면 **컴파일러는 아무 말도 안 한다.** 이런 실수는 테스트로만 잡힌다

## 1. 시나리오 실행 결과

```bash
for f in data/scenarios/0*.log; do echo "== $f"; ./build/bcm_sim -f $f | grep -E "STATE|잠금 명령|상태 전이"; done
```

| 시나리오 | 상태 전이 로그 | `fsm-design.md` 9번과 같은가 |
| --- | --- | --- |
| 01 정상 주행 | 1.8 IDLE → DRIVING_LOCKED (T3, 잠금 명령) → 9.5 → IDLE (T7). 전이 2회, 잠금 1회 | ✅ |
| 02 주행 중 문 열림 | 1.8 → DRIVING_LOCKED (T3, 잠금) → **4.0 → WARNING (T6)** → **5.0 → DRIVING_LOCKED (T9)** → 9.5 → IDLE (T7). 전이 4회, 잠금 1회 | ✅ |
| 03 차속 끊김 | 1.3 → DRIVING_LOCKED (T3, 잠금) → **3.5 → WARNING (T5, Fail-safe)** → **4.0 → DRIVING_LOCKED (T9)** → 7.5 → IDLE (T7). 전이 4회, 잠금 1회 | ✅ (3.4초가 아니라 3.5초) |
| 04 이상한 프레임 | 0.8 → DRIVING_LOCKED (T3, 잠금) → **1.8 → WARNING (T5, 차속 0xFFFF)** → **1.9 → DRIVING_LOCKED (T9)** → 3.8 → IDLE (T7). 전이 4회, 잠금 1회 | ✅ (M4-3에서 3.8초 IDLE을 문서에 추가) |

- 네 시나리오 모두 **잠금 명령은 딱 1회** → T9로 돌아올 때는 다시 잠그지 않는다 (Mealy, `door_lock_cmd = (rule == T3)`)
- 02와 03은 **원인이 다른데 모양이 같다**: DRIVING_LOCKED → WARNING → DRIVING_LOCKED. 로그의 규칙 번호(T6 / T5)와 이유(`문 열림` / `신호 이상`)로만 구별된다 → `BcmOutputs.rule`을 넣은 이유 (24번 노트 5번)
- 04의 깨진 줄(파싱 실패, DLC 부족, ID > 0x7FF)로는 상태가 한 번도 안 바뀌었다 → 디스패처가 이전 값을 지켜 준 덕분 (M2-6). FSM이 반응한 건 **형식은 맞는데 값이 "모름"**인 0xFFFF뿐
- 2.2초 기어 무효(`GEAR_INVALID`)에도 반응 없음 → 기어는 `inputs_ok`에 안 넣었으니까 (23번 노트 7번)

## 2. 전이 표 → 코드

```c
if (c.ign_stop) {               /* T1 */
    ...
} else {
    switch (prev) {
        case BCM_STATE_IDLE:    if (...) {...} else if (...) {...}   /* T2 ~ T4 */
        ...
    }
}
```

- T1을 `switch` 밖, 맨 앞에 둔 이유 → T1은 "**모든 상태**"에 해당하는 줄이라서. `switch` 안에 넣으면 `case IDLE`, `case DRIVING_LOCKED`, `case WARNING`마다 **똑같은 줄을 세 번** 써야 하고, 하나라도 빠뜨리면 그 상태에서는 시동을 꺼도 IDLE로 안 간다
  - `switch` 밖 맨 앞에 두면 **어떤 상태든 제일 먼저** 확인된다 → 표의 "위에서부터"와 순서가 같다
  - 0번에서 직접 본 효과: DRIVING_LOCKED의 `case`가 비어 있을 때도 10.0초에 시동을 끄니 IDLE로 돌아왔다. `default:`(알 수 없는 상태)에서도 마찬가지 → **어떤 경우에도 빠져나갈 길**이 하나는 보장된다
- 한 상태 안에서 `if`가 아니라 `if - else if`로 쓴 이유 ("처음 맞는 줄 하나만") → if로만 적게되면 처음 맞는 줄 하나만 적용되는것이 아니라, 맞는 줄 모두가 적용되기 때문이다. ✅
  - 예: DRIVING_LOCKED에서 문 열림 + 차속 끊김이면 T5와 T6이 **둘 다** 맞는다. `if`만 쓰면 T5가 `rule = T5`로 정한 뒤 T6이 `rule = T6`으로 **덮어써서**, 마지막에 맞은 줄이 이긴다 → 표의 우선순위(위가 이김)와 **반대**가 된다 (`test_priority`가 실패)
  - `else if`면 T5가 맞는 순간 나머지는 아예 확인하지 않는다 → "위에서부터 처음 맞는 하나만"이 코드 모양 그대로 보인다
- T2, T8은 상태를 안 바꾸는데 `rule`만 정하는 이유 → 
  - `rule`을 정하는 건 말한 대로 **"왜 그대로인지" 알 수 있게** 하려고다. 테스트에서 `ASSERT_EQ(BCM_RULE_T2, out.rule)`로 "아무 규칙도 안 맞아서 그대로"(NONE)와 "신호를 못 믿어서 일부러 그대로"(T2)를 **구별**한다 (`test_t2_*`, `test_t8_*`, `test_priority`)
  - 더 중요한 건 이 줄이 **아래 줄을 막는 문지기**라는 것. `else if`라서 T2가 맞으면 T3, T4는 **확인조차 안 한다.** 차속을 모르는데(`speed_known` 거짓) `c.speed >= 1500`을 확인하면 **옛날 값이나 무효값**으로 잠글 수 있다 → T2가 먼저 걸러서, 아래 줄들은 "신호는 믿을 만하다"고 가정하고 쓸 수 있다 (23번 노트 5번)
  - 그래서 이 줄은 `rule`이 없어도(`{ }` 빈 칸이어도) 지울 수 없다. 지우면 신호가 끊겼을 때 IDLE에서 잠그거나, WARNING에서 T9로 빠져나갔다가 다시 T5로 들어오는 왔다 갔다가 생긴다

## 3. 조건을 먼저 계산하기 (`BcmConditions`, `evaluate`)

- `switch` 안에서 `in->speed.centi_kmh` 같은 걸 직접 쓰지 않고 `c.speed`, `c.inputs_ok`를 먼저 만들어 둔 이유 → **코드가 전이 표와 똑같이 읽히게 하려고**

```c
/* 조건을 먼저 만들어 두면: 표의 "조건" 칸과 글자가 거의 같다 */
} else if (c.ign_run && c.speed >= BCM_LOCK_SPEED_CENTI && c.all_closed) {      /* T3 */

/* 직접 쓰면: 같은 T3인데 무슨 뜻인지 한참 봐야 한다 */
} else if (in->ign_received && now_ms - in->ign_rx_ms <= 500u
           && (in->ign.ign == IGN_ON || in->ign.ign == IGN_START)
           && in->speed_received && now_ms - in->speed_rx_ms <= 500u && in->speed.valid
           && in->speed.centi_kmh >= 1500u
           && in->doors_received && now_ms - in->doors_rx_ms <= 500u && !door_any_open(&in->doors)) {
```

  - **한 번만 정의하고 여러 번 쓴다:** `inputs_ok`는 T2, T5, T8 세 곳에서 쓴다. 직접 쓰면 "신호를 믿을 수 있나?" 계산을 세 번 복사해야 하고, 타임아웃 규칙을 바꿀 때 하나를 빠뜨리면 상태마다 기준이 달라진다
  - **문서와 1:1로 맞는다:** `fsm-design.md` 3번 표의 이름(`ign_run`, `all_closed`, ...)이 그대로 `BcmConditions`의 필드다. 문서의 정의를 바꾸면 `evaluate()` 한 곳만 고치면 된다
  - **계산과 판단을 나눈다:** `evaluate()` = "지금 상황이 어떤가" (입력 → 예/아니오), `switch` = "그럼 어떻게 할까" (예/아니오 → 상태). 23번 노트에서 "조건 이름을 먼저 정하고 표에서는 이름만 쓴다"고 한 것을 코드로 옮긴 것
  - 매 판단마다 전부 새로 계산한다 (시간이 흐르면 `speed_known` 답이 바뀌니까). 비교 몇 번이라 비용은 거의 없다
  - Java로 치면 복잡한 `if` 조건을 `boolean isMoving = ...;`처럼 이름 붙은 변수나 `isMoving()` 메서드로 빼는 리팩터링 ("설명 변수 도입", Introduce Explaining Variable)
- `signal_lost()`에서 `>`를 쓴 이유 (시나리오 03의 3.4초 / 3.5초) → 설계 문서의 정의가 "500ms를 **넘겼으면**(초과)" 끊김이기 때문. 코드는 문서와 같아야 한다

| 시각 | 마지막 차속(2.9초)부터 | `elapsed > 500` | `elapsed >= 500`이었다면 |
| --- | --- | --- | --- |
| 3.4초 | 500ms | **거짓** → 아직 정상 | 참 → 3.4초에 WARNING |
| 3.5초 | 600ms | **참** → WARNING (T5) | (이미 WARNING) |

  - 실행 결과도 `[3.500s] STATE DRIVING_LOCKED -> WARNING (T5)`로 문서와 같았다 (1번 표)
  - `>`와 `>=` 중 무엇이 "정답"인지는 정하기 나름이다. 중요한 건 **한 번 정하면 문서, 코드, 테스트가 모두 같은 것**을 쓰는 것. 그래서 `test_t5_fail_safe`에 **딱 500ms(아직 정상)**와 **600ms(끊김)**를 둘 다 넣어서 경계를 못 박았다
  - 이렇게 경계값(500 근처)에서 `>`와 `>=`가 헷갈리는 실수를 **off-by-one** 오류라고 한다. 경계값 테스트가 이걸 잡는다 (M2 디코더의 DLC, M4-4의 14.9 / 15.1 km/h도 같은 이유)
  - 덤: `elapsed`를 `now_ms >= rx_ms ? now_ms - rx_ms : 0`으로 구한 이유 → 둘 다 `uint32_t`(부호 없음)라서, 혹시 `now_ms`가 더 작으면 빼기 결과가 음수가 아니라 **아주 큰 수**(약 42억)로 돌아간다 (부호 없는 정수의 wraparound). 그러면 방금 받은 신호를 "끊김"으로 착각한다

## 4. `transition_to()`

- 상태를 바꾸는 곳을 한 함수로 모은 이유 (이슈: "로그·entry action 일원화") → **상태가 바뀔 때 항상 같이 해야 하는 일을 빠뜨리지 않으려고.** `ctx->state = ...`를 코드 곳곳에서 직접 쓰면, 상태를 바꾸는 길이 여러 개가 되고 "바꿀 때 같이 할 일"을 길마다 따로 챙겨야 한다

```
switch 안 (T1 ~ T11, RESET): "다음 상태는 무엇인가"만 정한다 → next, rule
          │
          ▼
transition_to(): "상태를 바꿀 때 하는 일"을 한 곳에서 → locked 켜기/끄기, ctx->state = next
```

  - **entry action**(진입 동작) = 어떤 상태에 **들어갈 때** 항상 하는 일. 이 설계에서는 "IDLE에 들어가면 `locked`를 지운다"가 entry action이다
  - 나중에 할 일이 늘어도 여기 한 곳만 고치면 된다. 예: M4-8에서 "상태가 바뀌면 0x400을 바로 한 번 보낸다"를 넣는다면 `transition_to()`에 한 줄
  - Spring으로 치면 `order.setStatus(...)`를 서비스 곳곳에서 부르지 않고, 엔티티의 `order.changeStatus(next)` 메서드 하나로만 바꾸게 하는 것 (상태 변경 + 이력 기록 + 이벤트 발행을 그 안에서)
- `locked`를 `case`마다 지우지 않고 `transition_to()`에서 "IDLE로 가면 지운다"로 한 번만 쓴 장점 → IDLE로 가는 길이 **5개**(T1, T7, T10, T11, RESET)라서. `case`마다 쓰면 5군데에 `ctx->locked = false;`를 복사해야 하고, 하나만 빠뜨려도 "옛날 잠금 기억이 남아서 다음 주행 때 잠금 명령 없이 잠김 상태가 되는" 버그가 생긴다

| | `case`마다 지우기 | `transition_to()`에서 한 번 |
| --- | --- | --- |
| 쓰는 곳 | 5군데 | 1군데 |
| IDLE로 가는 규칙을 새로 추가하면 | 그 줄에도 잊지 말고 써야 함 | **아무것도 안 해도** 지워짐 |
| 규칙을 읽을 때 | 상태 + 부수 효과가 섞여 있음 | `next`, `rule` 두 줄뿐 → 표와 똑같이 읽힘 |

  - 0번에서 T7을 채울 때 `locked`를 안 건드렸는데도 `test_t7_parked`의 `ASSERT_FALSE(ctx.locked)`가 통과한 게 이 덕분이다
  - 규칙("무엇으로 바꿀까")과 동작("바꿀 때 무엇을 할까")을 **나눈** 것. 같은 이유로 `locked = true`도 T3 줄이 아니라 `transition_to()`에 있다
- 로그는 `bcm.c`가 아니라 `log_reader.c`(`log_bcm`)에서 찍게 했다. 왜 그렇게 했을까 → **`bcm.c`를 "판단만 하는" 순수한 로직으로 남겨 두려고.** 이슈에는 `transition_to()`에서 로그를 찍으라고 되어 있지만, 일부러 바꿨다

| | `bcm.c` 안에서 `LOG_INFO` | `BcmOutputs`로 알려 주고 밖에서 찍기 (이번 방식) |
| --- | --- | --- |
| `bcm.c`가 아는 것 | 로거, 출력 형식, 로그 레벨까지 | 상태와 규칙뿐 |
| 테스트할 때 | 91개 테스트마다 로그가 화면에 찍힘 → 매번 `log_set_level(OFF)` 필요 | 조용함. `out.state_changed`, `out.rule`을 `ASSERT`로 확인 |
| 실시간 모드(M5) | 로그 형식을 바꾸려면 BCM을 고쳐야 함 | 부르는 쪽에서 마음대로 (로그, 0x400 송신, 화면 표시) |

  - 그래서 24번에서 `prev_state`, `state_changed`, `rule`을 `BcmOutputs`에 넣어 둔 것이다 → 로그에 필요한 정보는 BCM이 **결과로 알려 주고**, 찍는 건 부르는 쪽(`log_bcm`)이 한다
  - 이슈의 목적("상태가 바뀔 때마다 로그")은 그대로 지켰다: `[4.000s] STATE DRIVING_LOCKED -> WARNING (T6: 문 열림, 3km/h 초과)`
  - Spring으로 치면 도메인 엔티티 안에서 직접 로그를 찍거나 메일을 보내지 않고, **도메인 이벤트**를 돌려주면 서비스나 리스너가 처리하는 것. 핵심 로직이 바깥(로거, 메일 서버)에 의존하지 않게

## 5. `default:`와 `-Wswitch-enum`

- `default:`에서 IDLE로 보내는(`BCM_RULE_RESET`) 이유. `ctx->state`가 0~2가 아닌 값이 될 수 있나? → **될 수 있다.** C의 enum은 그냥 정수라서, 아무 숫자나 들어갈 수 있다

| 어떻게 이상한 값이 들어가나 | 예 |
| --- | --- |
| `bcm_init`을 깜빡함 | `BcmContext ctx;`만 쓰면 지역 변수라 **쓰레기 값** (초기화 안 된 지역 변수) |
| 다른 코드가 메모리를 덮어씀 | 옆에 있는 배열을 넘쳐서 쓰면(buffer overflow) `ctx.state`까지 망가진다 |
| 하드웨어 오류 | 차량 MCU에서는 전기 노이즈나 우주선(cosmic ray)으로 메모리 비트 하나가 뒤집히는 일이 실제로 있다 (SEU, 기능 안전에서 다룸) |
| 잘못된 형 변환 | `ctx.state = (BcmState)raw;`처럼 검사 없이 숫자를 넣음 |

  - 이럴 때 `default:`가 없으면 **어느 `case`에도 안 걸려서 아무것도 안 하고** 그 이상한 상태에 영원히 머문다. T1만 빠져나갈 길이 되는데, 시동을 끄기 전까지 BCM이 멈춘 것과 같다
  - 그래서 "모르면 안전한 곳으로": **IDLE**(아무것도 안 하는 상태)로 보낸다. `locked`도 지운다 → Fail-safe와 같은 생각 (23번 노트 7번). "정의 안 된 조합 = 버그"(M4-1 이슈)의 마지막 안전망
  - `BCM_RULE_RESET`으로 이유를 남겨서, 로그에 `STATE UNKNOWN -> IDLE (RESET: 알 수 없는 상태, 안전 상태로)`가 찍힌다 → 정비할 때 "메모리 이상이 있었다"는 흔적이 된다 (`test_unknown_state_resets`에서 `(BcmState)7`로 확인)
- 실험: `case BCM_STATE_WARNING:` 줄부터 그 `break;`까지 잠깐 주석 처리하고 `make` → 나온 메시지 → (복사본으로 직접 해 봄)

```
bcm.c:87:17: error: enumeration value 'BCM_STATE_WARNING' not explicitly handled in switch [-Werror,-Wswitch-enum]
```

  - "`BCM_STATE_WARNING`을 `switch`에서 **직접** 처리하지 않았다"는 뜻. `-Werror`라 빌드 실패 → 상태를 하나 빠뜨린 걸 **컴파일할 때** 바로 안다
  - 같은 복사본을 `-Wswitch-enum` **없이**(`-Wall -Wextra`만) 컴파일하면 → **아무 경고 없이 성공**한다 😱 WARNING 상태가 `default:`로 떨어져서, 경고 중에 갑자기 IDLE로 리셋되는 버그가 조용히 들어간다
- 실험이 끝나면 되돌린다. `-Wswitch-enum`이 `-Wall`의 `-Wswitch`와 다른 점 → **`default:`가 있을 때도 경고하느냐**의 차이

| `switch`에 빠진 `case`가 있을 때 | `-Wswitch` (`-Wall`에 포함) | `-Wswitch-enum` (따로 켜야 함) |
| --- | --- | --- |
| `default:` **없음** | ⚠️ 경고 | ⚠️ 경고 |
| `default:` **있음** | 조용 | ⚠️ **경고** |

  - 작은 예제로 확인: `enum {A, B, C}`에서 `case A`, `case B`만 쓰고 `default:`를 넣으면 `-Wall`은 아무 말 안 하고, `-Wswitch-enum`만 `enumeration value 'C' not explicitly handled`를 낸다
  - 우리 `bcm_step`은 메모리 오류 대비로 `default:`를 **꼭** 넣어야 한다. 그런데 `-Wswitch`는 `default:`가 있으면 "다 처리했구나" 하고 입을 다문다 → 진짜 상태를 빠뜨려도 모른다. 그래서 M4-3에서 Makefile에 `-Wswitch-enum`을 추가했다
  - 두 가지를 함께 쓰는 게 요점: **`default:`는 실행 중의 이상한 값**을 막고, **`-Wswitch-enum`은 코드를 쓸 때의 실수**(case 빠뜨림)를 막는다
  - 상태가 하나 늘면(예: `BCM_STATE_PARKING`) 이 enum으로 `switch`하는 모든 곳(`bcm_step`, `bcm_state_name`)에서 경고가 나서 고칠 곳을 다 알려 준다. Java 21의 `switch`가 sealed/enum에서 빠진 case를 컴파일 에러로 알려 주는 것과 같은 효과

## 6. API 변경

- `log_read_stream(in, inputs, stats)` → `log_read_stream(in, inputs, bcm, stats)`. `bcm`에 `NULL`을 허용한 이유 → **BCM 없이 "읽기만" 하고 싶은 곳이 있기 때문**
  - `test_log_reader.c`의 테스트 45개는 파서와 디스패처를 확인하는 테스트다. BCM까지 돌면 상태 전이 로그나 통계가 섞여서 확인하려는 것과 상관없는 결과가 생긴다 → `NULL`을 넘겨서 M3 때와 **똑같이** 동작하게 했다
  - 그래서 기존 테스트는 인자 하나(`NULL`)만 추가하고 **내용은 그대로** 통과했다 → API를 바꿔도 예전 동작이 깨지지 않았다는 증거
  - "**없어도 되는 것**은 `NULL` 허용"이 이 프로젝트의 관례다. `can_dispatch(..., stats)`의 `stats`, M3-3의 `FILE *err`(`NULL`이면 안 찍음), `cli_parse(..., err)`도 같은 방식
  - Java로 치면 `Optional<BcmContext>`나, 리스너를 등록 안 하면 그냥 안 부르는 것
- 덤: 이번 변경으로 줄 하나의 흐름이 이렇게 됐다

```
fgets → parse_candump_line → can_dispatch (VehicleInputs 갱신) → bcm_step (판단) → log_bcm (바뀌었으면 로그)
                              M3                                  M4-3 추가
```
- `main.c`에서 `bcm_init(&bcm)`을 `log_read_file` 전에 한 번만 부르는 이유 (24번 노트 1번) → `bcm_init`은 BCM을 **IDLE로 처음부터** 시작하게 하는 함수라서. 프레임마다 부르면 매번 기억을 지워서 BCM이 "기억상실"에 걸린다
  - 프레임마다 불렀다면: 1.8초 T3 잠금 → 1.9초 `bcm_init`으로 다시 IDLE → 또 T3 잠금 → ... **100ms마다 잠금 명령** (결과 요약의 "도어 잠금 명령 : 1회"가 수십 회가 된다)
  - 그래서 `main`에서 `vehicle_inputs_init(&in)`과 나란히 **루프 밖에서 한 번**만 부르고, 그 `bcm`을 `log_read_file`에 넘겨서 파일 하나를 읽는 동안 **같은 BCM**이 계속 기억을 들고 판단하게 했다
  - 파일을 다 읽은 뒤에도 `bcm`이 `main`에 남아 있어서, 결과 요약에서 `최종 상태 : IDLE`을 찍을 수 있다 (상태를 부르는 쪽이 들고 있는 설계의 장점, 24번 노트 3번)

## 7. 테스트

- 결과 → `[test_bcm]` **91 passed**, 0 failed. 전체 `make test` 통과 (맥, 리눅스 컨테이너, Sanitizer, `-Wswitch-enum` 포함 엄격 경고)

| 묶음 | 테스트 | 확인하는 것 |
| --- | --- | --- |
| 모양 (M4-2) | `test_state_values_match_0x400` 등 5개 | enum 값, 초기화, NULL, 이름 |
| T1 | `test_t1_ignition_off` | 모든 상태에서 OFF/ACC → IDLE, `locked` 지움 |
| IDLE | `test_t2_*`, `test_t3_*`, `test_t4_*` | 신호 없으면 대기, 15 km/h 잠금 (경계), 문 열고 출발 |
| DRIVING_LOCKED (직접 구현) | `test_t5_*`, `test_t6_*`, `test_t7_*` | Fail-safe 4가지 원인, 트렁크, 2 km/h, D/P/기어 무효 |
| WARNING | `test_t8_*` ~ `test_t11_*` | 신호 이상 유지, `locked`에 따른 복귀, 정차 |
| 그 밖 | `test_priority`, `test_unknown_state_resets`, `test_lock_cmd_only_once` | 우선순위 3쌍, `default:`, 잠금 명령 1회 |
- `driving()`, `step_from()` 도우미 함수를 만든 이유 → **테스트마다 "다른 부분"만 보이게** 하려고. 테스트 하나를 준비하려면 `VehicleInputs`의 필드 11개를 채워야 하는데, 매번 다 쓰면 정작 무엇을 확인하는지가 묻힌다

```c
/* 도우미 없이 */
VehicleInputs in; vehicle_inputs_init(&in);
in.ign.ign = IGN_ON; in.ign.gear = GEAR_D; in.speed.valid = true; in.speed.centi_kmh = 4000u;
in.ign_received = true; in.speed_received = true; in.doors_received = true;
in.ign_rx_ms = NOW; in.speed_rx_ms = NOW; in.doors_rx_ms = NOW;
in.doors.trunk = true;                               /* ← 이 테스트에서 진짜 중요한 건 이 한 줄 */

/* 도우미로 */
VehicleInputs in = driving(4000u);                   /* "정상 주행 중"에서 시작해서 */
in.doors.trunk = true;                               /* 다른 점만 */
```

  - `driving()` = "모든 신호가 방금 정상으로 들어온 주행 상태"라는 **기본값**. 테스트는 거기서 한두 군데만 바꾼다 (차속 끊김 → `in.speed_rx_ms = NOW - 600u`)
  - `step_from()` = given(원하는 상태와 `locked`로 시작) + when(`bcm_step` 한 번)을 한 줄로 (24번 노트 3번)
  - 기본값을 고칠 일이 생기면(예: M4-5에서 디바운싱용 필드가 추가됨) `driving()` 한 곳만 고치면 91개가 다 따라온다
  - JUnit의 테스트 픽스처 / Test Data Builder 패턴 (`OrderFixture.paid().withItem(...)`)과 같은 생각
- `test_t3_auto_lock`에서 1600, 1500, 1499를 모두 넣어 본 이유 → **경계값 테스트.** 조건이 `speed >= 1500`일 때 버그는 대부분 **경계에서** 생긴다

| 값 | 의미 | 기대 | 이 값이 잡는 실수 |
| --- | --- | --- | --- |
| 1600 (16.00 km/h) | 확실히 넘음 | 잠금 | 아예 잠그지 않는 버그 |
| **1500** (15.00 km/h) | **딱 경계** | 잠금 (`이상`이라서) | `>=`를 `>`로 잘못 씀 (off-by-one) |
| 1499 (14.99 km/h) | 경계 바로 아래 | 안 잠금 | `>= 1499`, `>= 1490`처럼 기준을 잘못 씀 |

  - 1600 하나만 넣으면 `>`로 써도, `>=`로 써도 통과한다 → 경계가 맞는지 모른다. **경계, 경계 바로 아래**를 같이 넣어야 기준이 정확히 15.00인지 못 박힌다
  - 같은 방식으로 `test_t4_*`는 3.00 km/h(초과가 아님), `test_t5_*`는 500ms(아직 정상) / 600ms(끊김)를 넣었다 (3번의 `>` 이야기)
  - centi 단위 덕분에 14.99처럼 **정확한 값**을 넣을 수 있다. float였다면 15.0이 14.999999로 저장되는 문제 때문에 경계 테스트가 흔들렸을 것 (M1-4 고정 소수점)
