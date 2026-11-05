# 29. FSM 표 기반 테스트 & 시나리오 통합 테스트 (M4-7)

> 코드: `tests/test_bcm_table.c` (★ T8 ~ T11 줄 직접 채울 곳), `tests/test_scenarios.c`, `include/log_reader.h`, `src/log_reader.c`
> 실행: `make test` (`[test_bcm_table]`, `[test_scenarios]`)

## 0. 직접 채우기: 전이 표의 T8 ~ T11 줄

`tests/test_bcm_table.c`에서 `TODO(직접 채우기)`를 찾는다. `docs/fsm-design.md` 4번 표와 23번 노트 1번 표(8 ~ 12번 상황)를 보면서 한 줄씩 쓴다.

1. **채우기 전에** `make test`를 실행해서 `[test_bcm_table]` 부분을 적는다 → `133 passed, 4 failed`. 실패는 전부 커버리지 검사에서 나왔다

```
    표에 T8가 없음
    FAIL tests/test_bcm_table.c:179: seen[r] 가 참이어야 함
    표에 T9가 없음
    표에 T10가 없음
    표에 T11가 없음
  (전이 표 25줄)
```

   - 표의 다른 25줄은 전부 통과. "틀린 줄"이 아니라 "**없는 줄**"을 알려 준 것
2. T8, T9, T10, T11 네 줄을 쓴다 → 쓴 결과 (마지막 두 칸은 아래 "막혔던 곳"에서 고침)

```c
{ "T8 신호 이상이면 경고 유지", WARN, true,  IGN_ON, GEAR_D, KMH(50u), CLOSED, SPEED_LOST, WARN,                     BCM_RULE_T8,  false },
{ "T9 40km/h, 문 전부 닫힘",    WARN, true,  IGN_ON, GEAR_D, KMH(40u), CLOSED, OK,         BCM_STATE_DRIVING_LOCKED, BCM_RULE_T9,  false },
{ "T10 20km/h, 문 전부 닫힘",   WARN, false, IGN_ON, GEAR_D, KMH(20u), CLOSED, OK,         BCM_STATE_IDLE,           BCM_RULE_T10, false },
{ "T11 0km/h, FL 열림",         WARN, false, IGN_ON, GEAR_P, KMH(0u),  FL,     OK,         BCM_STATE_IDLE,           BCM_RULE_T11, false },
```
3. **채운 뒤에** `make test` → ✅ `[test_bcm_table]` **157 passed, 0 failed** (전이 표 29줄). 전체 `make test` 통과
4. 일부러 한 줄을 틀리게 써 본다 (예: T10 줄의 `locked`를 `true`로). 어떤 메시지가 나오나 → (확인 후 되돌린다) → (복사본으로 확인) `155 passed, 2 failed`

```
    FAIL tests/test_bcm_table.c:163: out.state
         기대: 0 (0x0)          ← 표에 적은 IDLE
         실제: 1 (0x1)          ← BCM은 DRIVING_LOCKED로 감
    FAIL tests/test_bcm_table.c:164: out.rule
         기대: 10 (0xA)         ← 표에 적은 T10
         실제: 9 (0x9)          ← BCM은 T9를 실행함
         ↑ 표 26번째 줄 [T10 20km/h, 문 전부 닫힘]
```

   - `locked`를 `true`로 바꾸면 입력이 **T9의 상황**이 된다(문 닫힘 + 잠갔었음). BCM은 맞게 T9를 실행했는데, 표는 T10을 기대하니 어긋난다
   - 메시지를 읽는 법: `기대`는 **표에 적은 값**, `실제`는 **BCM이 한 일**, `↑`는 **몇 번째 줄**인지. 숫자는 enum 값이라 0 = IDLE, 1 = DRIVING_LOCKED, 2 = WARNING / 규칙은 9 = T9, 10 = T10 (24번 노트 4번에서 T 번호와 숫자를 맞춰 둔 덕분에 바로 읽힌다)

- 막혔던 곳 / 처음에 틀렸던 것 → 세 단계로 막혔다
  - **T10 (기대값을 틀림):** T10은 "안 잠갔었으면 → **IDLE**". 잠금 주행으로 돌아가는 건 잠갔었을 때(T9). T9와 T10은 `locked` 하나로 갈리는 쌍이다. IDLE은 "멈춤"이 아니라 "**아직 안 잠근 상태**"라서, 20 km/h로 달려도 IDLE로 간다 → 다음 프레임에 T3이 정식으로 잠근다
  - **T11 (입력이 다른 규칙을 건드림):** `IGN_OFF`면 **T1이 먼저** 걸린다 (위에서부터 처음 맞는 줄). 상태는 IDLE로 맞게 갔는데 **규칙이 T1**이라 실패. T11을 테스트하려면 T1이 안 걸리게 `IGN_ON`
  - 교훈: 표 기반 테스트에서 **결과(상태)만** 확인했다면 T11 줄은 통과했을 것이고, 사실은 T1을 테스트하고 있다는 걸 몰랐을 것이다. **`rule`까지** 확인하니까 "테스트하려던 규칙을 진짜로 테스트하고 있나"까지 지켜진다
  - T8, T9는 처음 쓴 그대로 통과

## 1. 표 기반 테스트란

```c
static const BcmCase cases[] = {
    /* name                     from    lock  ign     gear    speed     doors   faults      to      rule         lock_cmd */
    { "T3 16 km/h 문 닫힘 → 잠금", IDLE,   false, IGN_ON, GEAR_D, KMH(16u), CLOSED, OK,         LOCKED, BCM_RULE_T3, true  },
    { "T5 차속 끊김 (Fail-safe)",  LOCKED, true,  IGN_ON, GEAR_D, KMH(50u), CLOSED, SPEED_LOST, WARN,   BCM_RULE_T5, false },
    ...
};
for (i = 0; i < CASE_COUNT; i++) { ... bcm_step ... ASSERT_EQ(c->to, out.state); ... }
```

- 표 기반 테스트를 한 문장으로 → **"입력과 기대 결과"를 표(배열)의 한 줄로 적어 두고, 검사하는 코드는 반복문 하나로 모든 줄에 똑같이 돌리는 테스트.** 데이터(무엇을 확인할지)와 로직(어떻게 확인할지)을 나눈다
- M4-3의 `test_t3_auto_lock()` 같은 함수 하나하나와 비교해서 좋은 점 / 아쉬운 점 →

| | 함수 하나씩 (`test_bcm.c`) | 표 기반 (`test_bcm_table.c`) |
| --- | --- | --- |
| 경우 하나 추가 | 함수를 새로 쓰고 `RUN_TEST`에 등록 (10줄 안팎) | **한 줄** 추가 |
| 한눈에 보기 | 함수가 흩어져 있어서 전체 규칙이 안 보임 | 29줄이 **설계 문서의 표처럼** 보임 → 빠진 경우가 눈에 띈다 |
| 검사 방법 | 함수마다 다르게 쓸 수 있음 (실수로 하나만 덜 검사할 수도) | **모든 줄을 똑같이** 검사 (상태, 규칙, 잠금, 경고음, 바뀜 여부 5가지) |
| 잘 맞는 것 | **흐름**이 있는 테스트 (여러 번 판단, 히스테리시스, 디바운싱, 복귀) | **판단 한 번**으로 끝나는 테스트 (전이 표의 줄) |
| 아쉬운 점 | 비슷한 코드가 반복됨 | 표에 안 맞는 경우(여러 단계, 시간 흐름)는 못 넣는다. 칸이 많아지면 줄이 길어져 읽기 어렵다. 실패 위치가 "검사 코드의 줄"이라 표 줄을 따로 찍어 줘야 한다 (3번) |

  - 그래서 **둘 다** 쓴다: 전이 표의 "한 번 판단"은 표로, "시간에 따른 흐름"은 함수로 (`test_bcm.c`의 `run_jitter`, `tick_step` 테스트들)
- JUnit `@ParameterizedTest` + `@CsvSource`와 비교 →

```java
@ParameterizedTest(name = "{0}")
@CsvSource({
    "T3 16 km/h 문 닫힘, IDLE,   false, ON, D, 1600, CLOSED, OK,         DRIVING_LOCKED, T3, true",
    "T5 차속 끊김,       LOCKED, true,  ON, D, 5000, CLOSED, SPEED_LOST, WARNING,        T5, false",
})
void transition(String name, BcmState from, boolean locked, ..., BcmRule rule, boolean lockCmd) {
    ...
    assertEquals(to, out.state());
}
```

| JUnit | 이 프로젝트 |
| --- | --- |
| `@CsvSource` / `@MethodSource` | `static const BcmCase cases[]` |
| 매개변수로 한 줄씩 넣어 줌 | `for` 반복문으로 한 줄씩 꺼냄 |
| `name = "{0}"`으로 실패한 줄 이름 표시 | `printf("↑ 표 %zu번째 줄 [%s]")` 직접 |
| 줄마다 **별도 테스트**로 셈 (하나 실패해도 나머지 계속) | `ASSERT`는 실패해도 계속 진행하니 같은 효과 |

  - C에는 어노테이션이 없어서 프레임워크가 해 주던 일(반복, 이름 표시)을 **직접** 쓴다. 생각은 똑같다

## 2. 표가 읽기 쉬운 이유

- `CLOSED`, `FL`, `SPEED_LOST`, `KMH(16u)`, `LOCKED` 같은 짧은 이름을 만든 이유 → **한 줄에 11칸이 들어가니까, 칸 하나하나가 짧고 뜻이 바로 보여야** 표가 표로 읽힌다

```c
/* 짧은 이름 없이 */
{ "T6", BCM_STATE_DRIVING_LOCKED, true, IGN_ON, GEAR_D, 4000u, 0x10u, 0x00u, BCM_STATE_WARNING, BCM_RULE_T6, false },
/* 짧은 이름으로 */
{ "T6", LOCKED,                   true, IGN_ON, GEAR_D, KMH(40u), TRUNK, OK,  WARN,              BCM_RULE_T6, false },
```

  - `0x10u`는 무슨 문인지 계산해야 하지만 `TRUNK`는 바로 읽힌다. `4000u`보다 `KMH(40u)`가 "40 km/h"로 읽힌다 (매직 넘버 대신 이름, M1-1과 같은 이유)
  - 여러 개를 합칠 수 있게 비트로 정했다: 문 두 개 열림 = `FL | RR`, 신호 두 개 이상 = `SPEED_LOST | DOORS_LOST`
  - 이 이름들은 **이 테스트 파일 안에서만** 쓴다 (`#define`이 파일 안에 있다). 제품 코드의 이름(`BCM_STATE_...`)을 바꾸는 게 아니라 테스트를 읽기 좋게 하는 별명
- `fsm-design.md`의 4번 표와 `cases[]`를 나란히 놓으면 무엇이 보이나 → **설계 문서의 한 줄이 테스트의 한 줄 이상과 짝지어진다**

```
fsm-design.md 4번 표                              tests/test_bcm_table.c
| T6 | DRIVING_LOCKED | door_open && speed > 3 | WARNING |    { "T6 트렁크 열림 40 km/h", LOCKED, ..., TRUNK, OK, WARN, BCM_RULE_T6, false },
                                                            { "-  문 열림 2 km/h는 유지", LOCKED, ..., FL, OK, LOCKED, BCM_RULE_NONE, false },
```

  - 규칙이 맞는 경우(T6) **옆에** 경계 바로 아래(2 km/h, 규칙 없음)가 짝으로 붙어 있다 → "되는 것"과 "안 되는 것"을 같이 본다 (27번, 28번 노트의 "짝 테스트")
  - 설계 문서를 고치면 이 표에서 **같은 줄**을 찾아 고치면 된다. 문서 ↔ 테스트 ↔ 코드가 같은 T 번호로 이어져 있다
  - 문서에 없는 경우(`RESET`, "잠금 주행 중 다시 16 km/h")도 표에 있다 → 문서의 "표에 없는 조합"(23번 노트 9번)을 테스트로 못 박은 것

## 3. 실패 메시지

- `ASSERT_EQ`는 파일:줄만 알려 준다. 반복문 안에서는 모든 줄이 같은 `ASSERT_EQ` 줄이라 "몇 번째 표 줄이 틀렸나"를 알 수 없다. 그래서 무엇을 했나 → 그 줄 검사에서 실패가 하나라도 생겼으면 **표의 몇 번째 줄인지와 이름**을 덧붙여 찍었다

```
    FAIL tests/test_bcm_table.c:163: out.state       ← 검사 코드의 줄 (29줄 전부 같은 163번)
         기대: 0 (0x0)
         실제: 1 (0x1)
         ↑ 표 26번째 줄 [T10 20km/h, 문 전부 닫힘]    ← 덧붙인 것: 어느 표 줄인지
```

  - 각 줄에 `name`을 둔 이유가 이것. 번호만 있으면 세어 봐야 하지만 이름이 있으면 바로 안다
  - 시나리오 통합 테스트도 같은 방법으로 `↑ [data/scenarios/02_...log]`와 **실제 전이 목록 전체**를 찍는다
- `failed_before = tf_failed` → 확인 → `tf_failed != failed_before`로 비교하는 방식이 하는 일 → **"이 줄을 검사하는 동안 실패가 늘었나?"**를 알아내는 것

```c
int failed_before = tf_failed;      /* ① 검사 전 실패 개수를 기억 (예: 0) */
ASSERT_EQ(c->to, out.state);        /* ② 5가지 검사. 틀리면 tf_failed++ */
...
if (tf_failed != failed_before) {   /* ③ 개수가 늘었으면 = 이 줄에서 뭔가 틀렸음 */
    printf("↑ 표 %zu번째 줄 [%s]\n", ...);
}
```

  - `tf_failed`는 `test_framework.h`에 있는 "지금까지 실패한 개수" 전역 변수. `ASSERT_*`가 틀릴 때마다 1씩 늘린다
  - 검사 5개 중 몇 개가 틀려도 `↑` 줄은 **한 번만** 찍힌다 (줄마다 한 번)
  - 테스트 프레임워크를 고치지 않고, 이미 있는 숫자를 "전과 후"로 비교해서 필요한 정보를 얻은 것 (M3-5 로그의 `before` 사진과 같은 방법)

## 4. 커버리지 검사 (`test_table_covers_every_rule`)

- 이 테스트가 확인하는 것 → **표의 `rule` 칸에 T1 ~ T11과 RESET이 전부 한 번 이상 나오는가.** 즉 설계 문서의 모든 규칙이 **최소 한 줄씩** 테스트되고 있는가

```c
bool seen[BCM_RULE_RESET + 1] = {false};
for (각 줄) seen[줄의 rule] = true;              /* 표에 나온 규칙에 표시 */
for (T1 ~ RESET) ASSERT_TRUE(seen[r]);           /* 표시 안 된 규칙이 있으면 실패 */
```
- 이게 없으면 0번에서 T8 ~ T11 줄을 안 써도 `make test`는 어떻게 되나 → **통과한다.** 표에 있는 25줄은 전부 맞으니까. 테스트는 "**있는 줄**"만 검사하지, "**없는 줄**"은 모른다
  - 0번 1단계에서 실패 4개가 전부 이 검사에서 나온 이유. "틀린 게 없다"와 "빠진 게 없다"는 다른 질문이다
  - 27번 노트의 교훈("통과했다 ≠ 지키고 있다")과 같은 이야기를 **표 전체**에 적용한 것
- 이슈의 완료 조건 "전이 표의 모든 행에 대응하는 테스트 존재"와의 관계 → 이 완료 조건을 **사람이 눈으로 확인하는 대신 `make test`가 매번 자동으로** 확인하게 만든 것
  - 나중에 규칙을 추가하면(T12) `BCM_RULE_RESET` 앞에 enum이 늘고 → 이 검사가 "표에 T12가 없음"으로 알려 준다 → 테스트를 깜빡할 수 없다
  - 한계: "규칙마다 **한 줄 이상**"만 본다. 그 줄이 **좋은** 테스트인지(경계값, 짝)는 못 본다. 그건 사람이 표를 읽고 판단한다

## 5. 시나리오 통합 테스트 (`test_scenarios.c`)

- 단위 테스트(`test_bcm`, `test_bcm_table`)와 무엇이 다른가 → **부품 하나가 아니라 조립된 전체를 진짜 데이터로 돌린다**

| | 단위 / 표 기반 | 시나리오 통합 |
| --- | --- | --- |
| 무엇을 부르나 | `bcm_step()`만 | `log_read_file()` → 파서 → 디스패처 → BCM (실제 프로그램과 같은 길) |
| 입력 | 테스트가 직접 만든 `VehicleInputs` | 실제 `.log` 파일 (7개, 약 1,900줄) |
| 찾는 버그 | BCM 규칙 하나가 틀림 | 부품 **사이**의 문제: 프레임 순서, 시각 계산, 디스패처와 BCM의 연결, 디바운싱과 복귀의 상호작용 |
| 예 | "T6에서 경고음이 켜지나" | "02에서 4.2초에 T6이 일어나나" |

  - M4-6에서 실제로 겪은 것: 단위 테스트 도우미가 프레임을 한꺼번에 넣어서 **실제와 다르게** 돌았다. 통합 테스트는 진짜 파일을 진짜 순서대로 읽으니 이런 차이가 없다
  - 시나리오 README에 "기대 결과"를 적어 두고 손으로 실행해서 눈으로 비교하던 것(M3-4 ~ M4-6)을 **자동화**한 것 = 인수 테스트(acceptance test)
- 시각까지(예: 02의 4200ms) 비교하게 한 이유 → **"언제"가 기능의 일부이기 때문.** 상태와 횟수만 보면 디바운싱, 복귀 조건, 타임아웃이 고장 나도 못 잡는다

| 망가진 것 | 상태 순서 (IDLE → LOCKED → WARN → LOCKED → IDLE) | 시각 |
| --- | --- | --- |
| 디바운싱이 1프레임 | 같음 ❌ 못 잡음 | 4.2 → **4.0초** ✅ 잡음 |
| 타임아웃 기준 `>`를 `>=`로 | 같음 | 03의 3.5 → **3.4초** |
| 복귀 조건이 1프레임 (03) | 같음 | 4.2 → **4.0초** |

  - M4-4 ~ M4-6에서 넣은 기능들은 전부 "**얼마나 늦게 / 언제**" 반응하느냐를 정하는 것이라, 시각을 안 보면 테스트하는 의미가 없다
- 이걸 위해 `LogReadStats`에 `transitions[]`를 추가했다. `LOG_MAX_TRANSITIONS`(16)로 개수를 정해 둔 이유 → **`malloc` 없이 고정 크기 배열**로 두려고. 이 프로젝트는 임베디드 습관대로 동적 메모리를 쓰지 않는다 (M3-3의 `line[128]`과 같은 이유)
  - 크기를 정하면: 메모리가 항상 같다(16 × 16바이트 = 256바이트), 넘쳐도 죽지 않는다 → 17번째부터는 **기록은 안 하고 횟수(`bcm_transitions`)만** 센다 (`if (stats->bcm_transitions < LOG_MAX_TRANSITIONS)`)
  - 지금 시나리오는 최대 4번이라 16이면 충분하다. 시나리오 07을 복귀 1프레임으로 돌린 실험(12회)도 들어간다
  - Java였다면 `List<Transition>`에 `add()`하면 끝이지만, C에서는 "최대 몇 개"를 **먼저 정하고** 넘칠 때를 처리해야 한다
- 실험(확인됨): 디바운싱 횟수를 1로 바꾸면 이 테스트가 무엇을 알려 주나 → `123 passed, 8 failed`. 02와 06의 **시각**이 틀렸다고, 실제 전이 목록과 함께 알려 준다

```
    FAIL ... e->t_ms
         기대: 4200 (0x1068)
         실제: 4000 (0xFA0)
         ↑ [data/scenarios/02_door_open_while_driving.log]
           실제 1.800s IDLE -> DRIVING_LOCKED (T3)
           실제 4.000s DRIVING_LOCKED -> WARNING (T6)      ← 0.2초 빨라짐 = 디바운싱이 안 됨
           실제 5.000s WARNING -> DRIVING_LOCKED (T9)
           실제 9.500s DRIVING_LOCKED -> IDLE (T7)
         ...
         실제: 12 (0xC)                                    ← 06은 노이즈가 다 경고가 돼서 전이 12회
```

  - 상태 순서는 02에서 똑같았으니, 시각을 안 봤다면 02는 통과했을 것이다 (바로 위 질문)

## 6. 테스트 피라미드

| 층 | 이 프로젝트의 예 | 개수 | 빠르기 | 실패했을 때 원인 찾기 |
| --- | --- | --- | --- | --- |
| 단위 | `test_bcm.c` (34개 함수, 188 확인), `test_signals.c`, `test_can_signal.c` 등 | **가장 많음** (전체 약 1,000 확인) | 매우 빠름 | **쉬움**: 함수 이름이 곧 원인 ("`test_t5_fail_safe` 실패 → T5 문제") |
| 표 기반 | `test_bcm_table.c` (29줄, 157 확인) | 중간 | 매우 빠름 | 쉬움: `↑ 표 26번째 줄 [T10 ...]` |
| 통합 (시나리오) | `test_scenarios.c` (7개 파일, 131 확인) | **가장 적음** | 상대적으로 느림 (파일 7개, 약 1,900줄을 끝까지 읽음) | **어려움**: "02에서 4.0초" → 파서? 디스패처? 디바운싱? 복귀? 어디가 문제인지 더 찾아야 함 |

- 왜 아래층(단위)을 많이, 위층(통합)을 적게 두나 → **실패했을 때 원인을 바로 알 수 있고, 빠르고, 만들기 쉬운 쪽을 많이** 두기 위해

```
        ▲  통합 (시나리오 7개)       ← 적게: "전체가 맞게 맞물리나" 확인
       ▲▲▲ 표 기반 (29줄)
     ▲▲▲▲▲▲ 단위 (함수 수백 개)      ← 많게: "부품 하나하나가 맞나" 확인
```

  - 통합 테스트 하나가 실패하면 **어디가** 틀렸는지 다시 찾아야 한다. 단위 테스트가 촘촘하면, 통합 테스트가 실패할 때 단위 테스트도 같이 실패해서 **위치를 바로** 알려 준다 (직접 확인: 디바운싱을 1로 바꾸면 통합 테스트는 "02가 4.0초"라고만 알려 주지만, 단위 테스트는 `test_debounce_noise_ignored`, `test_debounce_third_frame_accepted` 등 **디바운싱 테스트 4개가 콕 집어** 실패한다 → "디바운싱이 문제"라는 걸 바로 안다)
  - 반대로 단위 테스트만 있으면 "부품은 다 맞는데 **연결**이 틀린" 문제를 못 잡는다 (M4-6의 프레임 순서 문제) → 위층도 꼭 필요하다
  - 이 프로젝트는 전부 합쳐 1초도 안 걸려서 빠르기 차이가 잘 안 느껴지지만(직접 재 보니 셋 다 0.01초 미만), 실무에서는 통합 테스트가 DB, 네트워크, 실제 하드웨어(HIL)를 쓰면서 **분~시간** 단위로 느려진다 → 그래서 개수를 아낀다
  - Spring으로 치면: 단위 = Mockito로 서비스 하나, 통합 = `@SpringBootTest` + Testcontainers. 같은 피라미드
