# 24. 차량 상태 컨텍스트 & 출력 구조체 (M4-2)

> 코드: `include/bcm.h`, `src/bcm.c`, `tests/test_bcm.c`
> 실행: `make test` (`[test_bcm]`이 보이면 OK)
>
> 이번 이슈는 **모양만** 만든다. `bcm_step`은 아직 상태를 안 바꾸는 껍데기이고, 전이 표는 M4-3에서 채운다

## 1. 헤더만 읽고 답하기

`include/bcm.h`만 보고 (`.c`는 보지 말고) 답한다. "헤더만 보고 사용법을 알 수 있나"가 이번 이슈의 완료 조건이다.

| 질문 | 답 |
| --- | --- |
| BCM을 쓰기 전에 처음 한 번 불러야 하는 함수는? | **`bcm_init(&ctx)`** (IDLE, `locked = false`로 시작). 프로그램 시작 때 **한 번만** |
| 프레임을 하나 처리할 때마다 부르는 함수와 그 순서는? (`can_dispatch`와의 순서) | **`can_dispatch` → `bcm_step`**. 디스패처가 `VehicleInputs`를 먼저 고쳐야 BCM이 최신 값으로 판단한다. `bcm_init`은 매번 부르면 안 된다 (부를 때마다 IDLE로 돌아가 상태를 잊는다) |
| 지금 경고음을 울려야 하는지 알려면 무엇을 보나? | `BcmOutputs`의 **`warning_buzzer`** ✅ (`out.warning_buzzer`) |
| 이번에 잠금 명령을 보내야 하는지 알려면 무엇을 보나? | `BcmOutputs`의 **`door_lock_cmd`** (`out.door_lock_cmd`). 결과는 전부 `BcmOutputs`에 있다. `BcmContext`의 `locked`는 "잠갔었다"는 **기억**이라 주행 내내 true이고, 명령은 T3 순간에만 true |
| `bcm_step`이 고치는 것과 읽기만 하는 것은? | **고치는 것:** `ctx`(다음 상태로), `out`(결과 채움) / **읽기만:** `in`(`const VehicleInputs *`), `now_ms`(값으로 받음). `const`가 붙은 포인터 = 읽기만 한다는 약속 |
| 15 km/h라는 숫자는 어디에 정의돼 있나? | `bcm.h`의 **`BCM_LOCK_SPEED_CENTI`** ✅ (`1500u` = 15.00 km/h, centi 단위) |

## 2. 입력 / 상태 / 출력 나누기

```
VehicleInputs ─┐
now_ms        ├─▶ bcm_step ─▶ BcmOutputs
BcmContext    ─┘      │
     ▲                │
     └────────────────┘
```

- `VehicleInputs`, `BcmContext`, `BcmOutputs`가 각각 무엇을 담나 →

| 구조체 | 담는 것 | 누가 채우나 | 비유 |
| --- | --- | --- | --- |
| `VehicleInputs` (입력) | 차에서 들어온 **신호의 최신 값** (차속, 도어, 시동/기어 + 받은 시각) | 디스패처 (`can_dispatch`) | 계기판에 보이는 숫자들 |
| `BcmContext` (상태) | BCM의 **기억** (`state`, `locked`). 다음 판단 때도 필요한 것 | `bcm_init`, `bcm_step` | BCM의 머릿속 메모 |
| `BcmOutputs` (출력) | 이번 판단의 **결과** (상태, 경고음, 잠금 명령, 규칙 번호) | `bcm_step` (매번 새로) | BCM이 내리는 지시서 |

- 셋을 하나의 구조체로 합치지 않고 나눈 이유 → **누가 쓰고, 얼마나 오래 사는지가 다르기 때문**

| | 쓰는 사람 | 수명 |
| --- | --- | --- |
| `VehicleInputs` | 디스패처 (BCM은 읽기만) | 프로그램 내내 (계속 갱신) |
| `BcmContext` | BCM만 | 프로그램 내내 (기억) |
| `BcmOutputs` | BCM이 쓰고, 바깥이 읽음 | **판단 한 번** (다음 판단 때 새로 채움) |

  - 합치면 BCM이 실수로 입력(차속)을 고치거나, 바깥이 BCM의 기억(`locked`)을 건드릴 수 있다 → 나누고 `const`를 붙여서 **컴파일러가 막게** 했다
  - **테스트가 쉬워진다:** "이 기억(ctx) + 이 입력(in) → 이 결과(out)"로 딱 떨어진다. 입력과 상태를 원하는 대로 만들어 넣고 결과만 확인하면 된다 (M4-7의 표 기반 테스트가 `{초기 상태, 입력, 기대 출력}` 모양인 이유)
  - **입력이 어디서 왔는지 몰라도 된다:** 파일이든(M3) vcan 소켓이든(M5) `VehicleInputs`만 채워 주면 BCM은 그대로 동작한다
  - 이런 함수를 "**순수 함수에 가깝다**"고 한다: 같은 ctx + 같은 입력이면 항상 같은 결과. 전역 변수나 파일, 시계를 몰래 읽지 않는다 (시각도 `now_ms`로 받는다)

- `in`은 `const VehicleInputs *`인데 `ctx`는 `const`가 없는 이유 → 수정해야 하기 때문에. ✅ `bcm_step`은 `ctx`를 **다음 상태로 고쳐야** 하고(`state`, `locked`), `in`은 **읽기만** 한다. `const`를 붙이면 `bcm_step` 안에서 실수로 `in->speed = ...`를 쓰면 컴파일 에러가 난다 → 입력은 디스패처만 고친다는 규칙을 컴파일러가 지켜 준다 (M0-6 `const`)
- Java/Spring과 비교 (DTO, Service, 엔티티) →

| 이 프로젝트 | Java/Spring | 이유 |
| --- | --- | --- |
| `VehicleInputs` | 요청 **DTO** (`@RequestBody`) | 밖에서 들어온 값. 서비스는 읽기만 |
| `BcmContext` | **엔티티** (DB에 저장된 주문의 `status`) | 요청과 요청 사이에 남는 상태 |
| `BcmOutputs` | 응답 **DTO** (`ResponseEntity`) | 이번 처리 결과. 매번 새로 만든다 |
| `bcm_step(ctx, in, now, out)` | **Service** 메서드 `order.process(request)` | 상태 + 입력 → 상태 변경 + 결과 |
| `const VehicleInputs *` | 불변 DTO (`record`, `final` 필드) | 실수로 고치지 못하게 |

  - 차이: Java는 결과를 `return`으로 돌려주지만, C는 구조체를 돌려주기보다 **`out` 포인터로 채워 주는** 방식을 많이 쓴다 (`parse_candump_line(line, &frame, ...)`, `decode_*(f, &out)`도 같은 방식). 호출하는 쪽이 상자를 준비하니 `malloc`이 필요 없다


## 3. 전역 변수 vs 컨텍스트 구조체

- 로거(M3-5)는 `static` 전역 변수로 상태를 뒀는데, BCM은 `BcmContext`를 부르는 쪽이 들고 있게 했다. 왜 다르게 했나 → **로거는 "도구"이고, BCM은 "테스트해야 할 핵심 로직"이기 때문**

| | 로거 | BCM |
| --- | --- | --- |
| 정체 | 어디서나 쓰는 **공용 도구** | 이 프로젝트의 **핵심 판단 로직** |
| 상태를 넘겨주면 | 로그 찍는 모든 함수에 매개변수가 붙는다 (M3-3 `FILE *err`가 그랬다) | `bcm_step`을 부르는 곳은 **한두 군데**뿐이라 넘겨주는 게 부담이 없다 |
| 테스트에서 | 레벨만 바꾸면 되니 전역이어도 괜찮다 | 상태를 **원하는 대로 만들어** 11개 규칙을 하나씩 확인해야 한다 |
| 여러 개 필요? | 프로그램에 하나면 충분 | 테스트마다, 시나리오마다 **새 BCM**이 필요하다 |

  - 전역이면 테스트 사이에 상태가 **남는다** (M3-5 5번). 한 테스트가 WARNING으로 끝나면 다음 테스트도 WARNING에서 시작한다 → 테스트 순서에 따라 결과가 바뀐다. `BcmContext`는 테스트마다 지역 변수로 새로 만드니 서로 영향이 없다
  - 임베디드 실무에서도 핵심 로직은 이렇게 **컨텍스트 구조체를 넘기는** 방식을 많이 쓴다. 실제 ECU 코드는 `static` 모듈 변수를 쓰는 경우도 흔한데(메모리 위치를 고정하려고), 그때도 테스트를 위해 "초기화 함수 + 상태를 바꾸는 테스트용 함수"를 따로 둔다
  - Java로 치면 로거는 `static final Logger`(어디서나 같은 것), BCM은 `new BcmStateMachine()`(필요할 때마다 새 객체). 싱글톤으로 만든 서비스는 테스트하기 어렵다는 것과 같은 이야기

- `test_outputs_follow_state`에서 `ctx.state = BCM_STATE_WARNING;`처럼 직접 넣을 수 있는 게 왜 좋은가 → **확인하고 싶은 상태에서 바로 테스트를 시작할 수 있어서.** WARNING의 규칙(T8~T11)을 테스트하려면 먼저 WARNING이어야 한다

```c
/* 직접 넣을 수 없다면: WARNING까지 "운전해서" 가야 한다 */
bcm_init(&ctx);
in에 시동 ON, 16 km/h, 문 닫힘을 넣고  bcm_step → DRIVING_LOCKED
in에 FL 열림을 넣고                    bcm_step → WARNING      ← 이제야 준비 끝
in에 FL 닫힘을 넣고                    bcm_step → T9 확인       ← 진짜 하고 싶었던 테스트

/* 직접 넣을 수 있으면: 한 줄로 준비 끝 */
ctx.state = BCM_STATE_WARNING;
ctx.locked = true;
in에 FL 닫힘을 넣고                    bcm_step → T9 확인
```

  - 준비 과정이 길면 T9를 테스트하다가 **T3이나 T6이 고장 나도** 실패한다 → 무엇이 틀렸는지 알기 어렵다. 직접 넣으면 **T9만** 확인한다
  - `locked`처럼 "운전해서는 만들기 어려운 조합"(예: WARNING인데 `locked = false`)도 바로 만들 수 있다 → 1번 표의 10번 상황
  - M4-7의 표 기반 테스트 `{초기 상태, 입력, 기대 상태}`가 이 덕분에 가능하다. JUnit에서 테스트 전에 엔티티를 원하는 상태로 만들어 두는 **given**(given-when-then)과 같다


## 4. enum 값을 직접 적은 이유

```c
typedef enum {
    BCM_STATE_IDLE           = 0,
    BCM_STATE_DRIVING_LOCKED = 1,
    BCM_STATE_WARNING        = 2
} BcmState;
```

- 안 적어도 0, 1, 2가 되는데 굳이 적은 이유 → **이 숫자가 0x400 프레임에 그대로 실려 나가는 "약속"이기 때문.** signal-spec.md에 `BcmState: 0 = IDLE, 1 = DRIVING_LOCKED, 2 = WARNING`이라고 정해져 있고, 계기판 같은 **다른 제어기**가 이 숫자를 보고 경고등을 켠다
  - 값을 안 적으면 순서만으로 숫자가 정해진다. 누가 나중에 상태를 **중간에 끼워 넣거나 순서를 바꾸면** 숫자가 몰래 바뀐다

```c
typedef enum { BCM_STATE_IDLE, BCM_STATE_PARKING /* 새로 추가 */, BCM_STATE_DRIVING_LOCKED, BCM_STATE_WARNING } BcmState;
/* → DRIVING_LOCKED가 1에서 2로, WARNING이 2에서 3으로 바뀜 → 계기판은 "잠금 주행"을 "경고"로 읽는다 😱 */
```

  - 값을 직접 적어 두면 순서를 바꿔도 숫자가 그대로다. 그리고 `test_state_values_match_0x400`이 한 번 더 지킨다
  - Java로 치면 enum을 DB에 저장할 때 `ordinal()`(순서 번호)로 저장하면 위험하고, `@Enumerated(EnumType.STRING)`이나 직접 정한 코드값을 쓰는 것과 같은 이유

- `BcmRule`에서 `BCM_RULE_T3`의 값이 3이 되게 맞춘 이유 → **설계 문서의 T 번호와 코드의 숫자를 같게 해서 헷갈리지 않으려고.** 로그에 `T3`이라고 찍히거나 디버거(gdb)에서 `rule = 3`이 보이면, 바로 `fsm-design.md`의 T3 줄을 찾아가면 된다
  - 맞추는 방법: 맨 앞에 `BCM_RULE_NONE = 0`(맞는 줄 없음)을 두면 그다음부터 자동으로 T1 = 1, T2 = 2, T3 = 3, ... 이 된다. NONE이 0번 자리를 차지해서 번호가 한 칸씩 밀린 덕분
  - NONE을 0으로 한 이유: `= {0}`으로 초기화하거나 아무것도 실행되지 않았을 때 자연스럽게 "없음"이 된다
  - 0x400처럼 밖으로 나가는 값은 아니라서 "약속"까지는 아니지만, **사람이 읽기 쉽게** 맞춘 것


## 5. `BcmOutputs`를 매번 처음부터 채우는 이유

- 지난번 결과가 남아 있으면 어떤 버그가 생길 수 있나 (힌트: `door_lock_cmd`) → **잠금 명령이 꺼지지 않고 계속 나간다.** `bcm_step`이 "T3일 때 `door_lock_cmd = true`"만 하고 나머지 때 `false`로 되돌리지 않으면:

```
1.8초  T3 실행 → out.door_lock_cmd = true          (잠금 명령 ✅)
1.9초  아무 규칙도 안 맞음 → door_lock_cmd를 안 건드림 → 아직 true 😱
2.0초  "                                    → 아직 true 😱
...    0x400을 보낼 때마다 "잠가라" → 100ms마다 철컥철컥
```

  - "한 번만"이어야 하는 Mealy 출력(fsm-design 6번)이 **계속 켜진 Moore 출력처럼** 되어 버린다
  - 그래서 `bcm_step`은 맨 끝에서 **모든 칸을 매번 다시 쓴다**: `out->door_lock_cmd = (rule == BCM_RULE_T3);` → T3이 아니면 자동으로 false
  - 부르는 쪽이 `BcmOutputs out;`을 초기화 없이 만들어도(쓰레기 값) 안전하다. `bcm_step`이 전부 덮어쓰니까
  - Java로 치면 응답 DTO를 필드에 두고 재사용하다가 지난 요청의 값이 섞여 나가는 버그. 그래서 요청마다 `new ResponseDto()`를 만든다

- `prev_state`, `state_changed`, `rule`은 BCM 동작에는 필요 없는데 왜 넣었나 → **"무슨 일이 왜 일어났는지"를 바깥에 알려 주려고.** 로그와 테스트에 쓴다

| 필드 | 쓰는 곳 | 예 |
| --- | --- | --- |
| `prev_state` + `state` | 로그: 어디서 어디로 | `STATE IDLE -> DRIVING_LOCKED` |
| `state_changed` | 로그: **바뀔 때만** 찍기 (M3-5의 "값이 바뀔 때만 INFO"와 같은 생각) | 318번 판단 중 바뀐 4~5번만 출력 |
| `rule` | 로그: 왜 바뀌었나 / 테스트: 맞는 규칙이 실행됐나 | `(T3)`, `ASSERT_EQ(BCM_RULE_T9, out.rule)` |

  - `rule`이 특히 테스트에 중요하다: T5와 T6은 둘 다 WARNING으로 가서 **상태만 보면 구별이 안 된다.** `rule`을 보면 "신호 이상(T5) 때문인지, 문 때문(T6)인지" 확인할 수 있다 (23번 노트 5번의 우선순위 테스트)
  - 이런 정보가 없으면 바깥에서 `before = ctx.state` 같은 걸 따로 저장해서 비교해야 한다 (M3-5 `log_dispatch`의 `before` 사진처럼). BCM이 직접 알려 주면 부르는 쪽이 간단해진다
  - 차량 SW에서는 "왜 그렇게 판단했나"를 나중에 추적할 수 있는 게 중요하다 (고장 진단, 사고 분석). 판단 근거를 남기는 습관


## 6. 껍데기 함수

```c
(void)now_ms;
```

- 이 줄의 뜻과, 없으면 어떻게 되나 → **"이 매개변수는 일부러 안 쓰는 거야"라고 컴파일러에게 알려 주는 줄.** 실제로 하는 일은 없다 (값을 읽고 버린다). 없으면 **빌드가 실패한다** (직접 확인):

```
bcm.c:13:66: error: unused parameter 'now_ms' [-Werror,-Wunused-parameter]
```

  - 껍데기 `bcm_step`은 아직 타임아웃 계산을 안 해서 `now_ms`를 **한 번도 안 쓴다.** `-Wextra`는 "받아 놓고 안 쓰는 매개변수"를 실수일 수 있다고 경고하고, 우리는 `-Werror`라 경고 = 실패
  - `(void)`는 형 변환(캐스트)이다. 값을 `void`(아무것도 아님)로 바꿔서 버린다 → "썼다"로 쳐 준다
  - `now_ms` 자체는 지금 시각이 맞고, 타임아웃 계산에 쓰는 것도 맞다. 다만 그건 M4-3에서 구현하고, 그때 이 줄은 지운다 (진짜로 쓰게 되니까)
  - Java에는 이런 경고가 기본으로 없다. IntelliJ가 회색으로 "unused"라고 표시해 주는 정도. `@SuppressWarnings("unused")`가 비슷한 역할

- 아직 구현도 안 했는데 헤더와 테스트부터 만드는 이유 → **"어떻게 쓰는지"를 먼저 확정하려고.** 헤더는 BCM과 바깥의 **약속**(인터페이스)이다. 약속이 정해지면:
  - **M4-3은 안쪽만** 채우면 된다. `log_reader.c`, `main.c`처럼 BCM을 부르는 쪽은 M4-3에서 전이 표가 바뀌어도 고칠 필요가 없다
  - **부르는 쪽을 미리 써 볼 수 있다.** 껍데기라도 컴파일이 되니까 `bcm_step`을 부르는 코드를 먼저 붙여서 흐름을 확인할 수 있다
  - **써 보면서 모양이 이상한 걸 일찍 발견한다.** 예: 테스트를 짜다 보니 "규칙 번호가 출력에 있어야 T5와 T6을 구별할 수 있다" → `rule` 필드를 넣었다. 구현 다 하고 알았으면 다시 고쳐야 한다
  - Java로 치면 `interface BcmService`를 먼저 정하고 구현 클래스는 나중에 만드는 것. 프론트엔드와 API 스펙(OpenAPI)을 먼저 합의하고 각자 개발하는 것과도 같다
  - 테스트를 먼저 쓰는 방식을 **TDD**(Test-Driven Development)라고 한다. 여기서는 완전한 TDD는 아니고, "모양을 확인하는 테스트"를 먼저 둔 것


## 7. 테스트

- 결과 → `[test_bcm]` **24 passed**, 0 failed (맥, 리눅스 컨테이너, Sanitizer, 엄격 경고 모두 통과)

| 테스트 | 확인하는 것 |
| --- | --- |
| `test_state_values_match_0x400` | 상태 값 = 0x400 정의서의 값 (0, 1, 2) |
| `test_rule_numbers_match_table` | `BCM_RULE_Tn` = n (문서의 T 번호와 같음) |
| `test_init` | 쓰레기 값을 넣어 둬도 `bcm_init` 후 IDLE, `locked = false` |
| `test_outputs_follow_state` | 껍데기 `bcm_step`이 출력을 상태에 맞게 채우는지, WARNING이면 경고음 (Moore) |
| `test_null_args` | NULL을 넣어도 죽지 않고 `out`을 건드리지 않는지 |
| `test_names` | 이름 함수, 범위 밖 값(3)은 `"UNKNOWN"` |

- `test_state_values_match_0x400`이 지키는 약속 → **"BcmState의 숫자는 0x400 정의서의 숫자와 같다"** (IDLE = 0, DRIVING_LOCKED = 1, WARNING = 2). 4번에서 본 것처럼 이 숫자는 계기판 같은 **다른 제어기와의 약속**이다
  - 누가 나중에 enum 순서를 바꾸거나 `= 1`을 지워도, 숫자가 바뀌는 순간 **이 테스트가 실패**해서 알려 준다. 코드 리뷰에서 놓쳐도 `make test`가 잡는다
  - 이런 테스트를 "**계약 테스트**"라고도 한다. 기능을 확인하는 게 아니라 "바깥과 정한 약속이 깨지지 않았나"를 확인한다. Spring에서 API 응답 JSON의 필드 이름이 안 바뀌었는지 확인하는 테스트와 같은 목적

- `test_null_args`에서 `out.state = BCM_STATE_WARNING;`을 미리 넣어 둔 이유 → **"NULL이 있으면 `out`을 건드리지 않는다"를 확인하려고.** 헤더에 "인자 중 하나라도 NULL이면 아무것도 안 한다"고 약속했으니 그걸 검사한다

```c
out.state = BCM_STATE_WARNING;      /* ① 표시를 해 두고 */
bcm_step(NULL, &in, 0u, &out);      /* ② ctx가 NULL → 아무것도 안 해야 함 */
ASSERT_EQ(BCM_STATE_WARNING, out.state);   /* ③ 표시가 그대로면 → 안 건드렸다 ✅ */
```

  - 만약 `bcm_step`이 NULL 검사를 빼먹고 `out`부터 채웠다면 `out.state`가 IDLE(0)로 바뀌어서 ③이 실패한다
  - **일부러 IDLE이 아닌 값**을 넣은 게 요점이다. IDLE(0)을 넣어 두면, `bcm_step`이 IDLE로 덮어써도 값이 같아서 **건드렸는지 알 수 없다**
  - `ctx`가 NULL인데 계속 진행했다면 `ctx->state`를 읽다가 **segfault**로 테스트가 죽었을 것이다 → "죽지 않는다"도 함께 확인한다
  - M3-2 파서(`out`은 성공했을 때만 채움), M2-6 디스패처(실패하면 이전 값 유지)와 같은 "실패하면 결과를 안 건드린다" 원칙

