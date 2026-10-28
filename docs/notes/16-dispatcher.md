# 16. 메시지 디스패처 (M2-6)

> 코드: `include/dispatcher.h`, `src/dispatcher.c`, `tests/test_dispatcher.c`, `src/main.c`
> 실행: `make && ./build/bcm_sim`, `make test`

## 1. 실행 결과

```
can-bcm-simulator v0.0.1

[   0 ms] RX 0x300 [8] 02 03 00 00 00 00 00 00    → OK
[ 100 ms] RX 0x100 [8] 10 27 00 00 00 00 00 00    → OK
[ 150 ms] RX 0x555 [8] AA 00 00 00 00 00 00 00    → IGNORED
[ 200 ms] RX 0x200 [8] 01 00 00 00 00 00 00 00    → OK
[ 250 ms] RX 0x100 [1] 00                         → ERROR

현재 차량 상태
  차속  : 100.00 km/h
  도어  : FL=1 FR=0 RL=0 RR=0 TRUNK=0
  시동  : IGN=ON GEAR=D
  통계  : ok=3 ignored=1 error=1
```

- 250ms의 깨진 프레임(`0x100 [1] 00`) 뒤에도 차속이 100.00 km/h인 이유 →
  - **어떻게:** DLC가 1이라 최소 길이(2바이트)보다 짧다 → `decode_vehicle_speed()`가 `false` → 디스패처가 `ERROR`를 돌려주고 **`in->speed`를 덮어쓰지 않는다** (임시 변수에만 디코딩했다가 성공했을 때만 반영, 5번). 그래서 100ms에 받은 100.00 km/h가 그대로 남는다
  - **왜:** 깨진 프레임 **하나 때문에** 멀쩡하던 차량 상태가 망가지면 안 된다. 차속이 갑자기 이상한 값이 되면 BCM이 잘못 판단(잠금 해제 등)할 수 있다. 차속 메시지는 100ms마다 다시 오니까, 깨진 건 버리고 다음 정상 프레임을 기다리면 된다

## 2. 디스패처란?

```
CanFrame ──can_dispatch()──┬─ 0x100 → decode_vehicle_speed → in->speed
                           ├─ 0x200 → decode_door_status   → in->doors
                           ├─ 0x300 → decode_ign_status    → in->ign
                           └─ 그 외 → 무시
```

- 한 줄 설명 → ID를 보고 그에 맞는 디코더를 불러, 결과로 차량 입력 상태(`VehicleInputs`)를 갱신한다
- 웹으로 치면? → **라우터**(Spring의 `DispatcherServlet`). URL을 보고 알맞은 컨트롤러로 요청을 보내는 쪽이다. 각 디코더가 컨트롤러에 해당한다

| Spring | 이 프로젝트 |
| --- | --- |
| `DispatcherServlet` (URL 보고 분배) | `can_dispatch()` (ID 보고 분배) |
| `@GetMapping("/speed")` 컨트롤러 | `decode_vehicle_speed()` |
| 매핑 없는 URL → 404 | 모르는 ID → `DISPATCH_IGNORED` |

- 왜 필요한가? (CAN이 브로드캐스트라는 점과 연결) → CAN은 브로드캐스트라 **모든 메시지가 한 입구로 섞여서** 들어온다 (차속, 도어, 시동, 그리고 상관없는 메시지까지). 누군가 ID를 보고 **분류**하고, 상관없는 건 **걸러내야** 한다. 디스패처가 없으면 디코더마다 "이게 내 메시지인가?"를 매번 직접 확인해야 하고, 받은 프레임을 어느 디코더에 넘길지 정할 곳이 없다

## 3. `VehicleInputs` — 차량 입력 상태

```c
typedef struct {
    VehicleSpeed speed;  DoorStatus doors;  IgnStatus ign;
    bool speed_received, doors_received, ign_received;
    uint32_t speed_rx_ms, doors_rx_ms, ign_rx_ms;
} VehicleInputs;
```

- `_received`가 필요한 이유 (시동 직후를 생각해 보기) → 아직 안 받았을 때 `speed.centi_kmh`는 초기값 0이라, 확인 없이 쓰면 "**정차 중(0 km/h)**"으로 착각한다. "0 km/h를 받았다"와 "아직 모른다"를 구분하려고 둔다
- `_rx_ms`(마지막 수신 시각)는 어디에 쓰나? (M4-6) → 마지막으로 정상 수신한 시각이다. 타임아웃 판단에 사용한다. ✅ 예: `now_ms - speed_rx_ms > 500`이면 차속 메시지가 500ms째 안 온 것 → "ESC가 죽었다"고 보고 Fail-safe (정의서의 타임아웃 규칙)
- M4 상태 머신이 CanFrame이 아니라 VehicleInputs만 보게 한 이유 →
  - **역할 분리:** 상태 머신은 "판단"만 하고, 비트·ID·DLC 같은 "해석"은 디스패처가 맡는다 (M1-4 ADR의 외부 형식 / 내부 표현 분리)
  - **테스트가 쉽다:** 상태 머신을 테스트할 때 CAN 프레임을 만들 필요 없이 `VehicleInputs`에 값을 바로 넣으면 된다 (M4-7)
  - **입력이 어디서 오든 같다:** 로그 파일(M3)이든 vcan0(M5)이든, 결국 같은 `VehicleInputs`가 된다

## 4. 처리 결과 3가지

| 결과 | 언제 | 상태 변화 | 예시 |
| --- | --- | --- | --- |
| `DISPATCH_OK` | 우리 메시지(0x100/0x200/0x300)이고 해석 성공 | 해당 값 + `_received` + `_rx_ms` 갱신 | `100#1027...` → 차속 100.00 km/h |
| `DISPATCH_IGNORED` | 우리가 관심 없는 ID | 없음 | `555#AA...`, 우리가 보낸 `400#...` |
| `DISPATCH_ERROR` | 우리 메시지인데 해석 불가, 또는 프레임 자체가 이상함 | 없음 (이전 값 유지) | `100#00`(DLC 1), DLC 9, NULL |

- `IGNORED`와 `ERROR`를 나눈 이유 → **IGNORED는 정상 상황**(CAN은 브로드캐스트라 남의 메시지가 계속 옴)이고 **ERROR는 이상 신호**다. 실차 버스에는 상관없는 메시지가 초당 수천 개라 IGNORED는 엄청 많이 쌓인다. 둘을 섞으면 진짜 문제(ERROR)가 숫자에 묻혀 안 보인다 → 통계(`DispatchStats`)를 따로 센다
- 0x400(우리가 보낸 BCM_STATUS)을 무시하는 이유 → 0x400은 **우리 BCM이 판단한 결과**를 다른 제어기에 알리려고 보내는 메시지다 (M4-8). CAN은 브로드캐스트라 로그 파일(M3)이나 리눅스 루프백(M5)으로 **우리 메시지가 다시 들어올 수 있다.** 이걸 입력으로 쓰면 "내가 한 말을 내가 다시 듣고 판단"하는 꼴이 된다 → 입력으로 쓰지 않는다
- 차속 `0xFFFF`(신호 무효)는 셋 중 무엇인가? 이유는? → **`DISPATCH_OK`.** 프레임은 정상적으로 해석했고(ID, DLC 문제없음), 그 안의 신호 값이 "모름"인 것이다 (M2-3: 반환 true + `valid = false`). 그래서 `speed.valid = false`로 **상태를 갱신**하고 `speed_rx_ms`도 갱신한다. "ESC가 살아서 '센서 고장'이라고 알려줬다"는 정보도 받은 것이기 때문이다. ERROR로 처리하면 무효 상태가 반영되지 않고 이전의 정상 차속이 남아서 **고장 난 걸 모른다**

## 5. 설계 포인트: 실패하면 이전 값 유지

```c
VehicleSpeed s;                         /* 임시 변수에 먼저 */
if (!decode_vehicle_speed(f, &s)) {
    return DISPATCH_ERROR;              /* in은 건드리지 않음 */
}
in->speed = s;                          /* 성공했을 때만 반영 */
```

- 임시 변수 없이 `decode_vehicle_speed(f, &in->speed)`로 바로 넣으면 무슨 위험이? → 디코더가 **중간까지 값을 써 놓고 실패**하면 `in->speed`가 **반쯤 바뀐 상태**로 남는다. 지금 디코더들은 검사를 전부 끝낸 뒤에 값을 쓰도록 짜서 괜찮지만, 그건 디코더 내부 사정이다. 나중에 디코더를 고치다가 "값을 쓰고 → 검사하고 → 실패"하는 순서가 되면 바로 상태가 오염된다. 임시 변수에 먼저 받으면 **디코더가 어떻게 짜여 있든 "전부 반영 아니면 아무것도 안 바뀜"**이 보장된다

```
도어 디코더가 fl, fr까지 쓰고 실패했다면 (가정)
  임시 변수 O → in->doors 그대로 (안전)
  임시 변수 X → in->doors.fl, fr만 새 값, 나머지는 옛날 값 (섞임!)
```

- 실패했을 때 `speed_rx_ms`도 안 바꾸는 이유 (타임아웃과 연결) → `_rx_ms`는 "**정상 값을 마지막으로 받은 시각**"이어야 한다. 깨진 프레임에도 시각을 갱신하면, 깨진 프레임만 계속 와도 "방금 받았네"가 되어 **타임아웃이 영원히 안 걸린다** → 실제로는 쓸 수 있는 차속이 한참 전 값인데 BCM은 그걸 최신 값으로 믿는다

```
100ms  정상 100.00 km/h   → rx_ms = 100
200ms  깨진 프레임         → (잘못 갱신하면 rx_ms = 200)
...    깨진 프레임만 계속   → rx_ms가 계속 최신 → 타임아웃 안 걸림 😱
       (올바르게 하면 rx_ms = 100 그대로 → 600ms에 타임아웃 → Fail-safe ✅)
```

- 웹으로 치면? (트랜잭션) → `@Transactional`처럼 **"전부 성공하면 반영(commit), 하나라도 실패하면 아무것도 안 바꿈(rollback)"**. 임시 변수에 디코딩 = 트랜잭션 안에서 작업, `in->speed = s` = commit, `return DISPATCH_ERROR` = rollback. 깨진 프레임 하나로 상태가 반쯤 바뀌는 일이 없다

## 6. switch vs 함수 포인터 테이블 (이슈 키워드)

- 지금 방식 (`switch (f->id)`)의 장단점 →
  - 👍 **한눈에 보인다.** ID별로 무엇을 하는지 한 함수 안에 다 있다
  - 👍 메시지마다 결과 타입(`VehicleSpeed`, `DoorStatus`, `IgnStatus`)과 저장할 곳이 달라도 각 `case`에서 자연스럽게 처리된다
  - 👎 메시지가 늘어날수록 `switch`가 길어진다 (실차 BCM은 수십~수백 개)
  - 👎 메시지를 추가할 때마다 디스패처 함수 본문을 고쳐야 한다
- 함수 포인터 테이블 방식이란? (M5-5에서 다시 나옴) → **"ID → 처리 함수"를 표(배열)로 만들고, 표에서 찾아서 부르는 방식.** 함수 포인터는 "함수의 주소를 담는 변수"다 (M0-3에서 본 포인터의 함수 버전)

```c
/* 처리 함수의 모양을 맞춘다: 프레임과 상태를 받고 성공 여부를 돌려준다 */
typedef bool (*Handler)(const CanFrame *f, VehicleInputs *in, uint32_t now_ms);

static const struct { uint32_t id; Handler handle; } routes[] = {
    { MSG_ID_VEHICLE_SPEED, handle_speed },
    { MSG_ID_DOOR_STATUS,   handle_door  },
    { MSG_ID_IGN_STATUS,    handle_ign   },
};

for (i = 0; i < 3; i++) {
    if (routes[i].id == f->id) {
        return routes[i].handle(f, in, now_ms) ? DISPATCH_OK : DISPATCH_ERROR;
    }
}
return DISPATCH_IGNORED;
```

  - 메시지를 추가할 때 **표에 한 줄**만 넣으면 된다 (디스패처 본문은 그대로)
  - Spring의 `@GetMapping`이 하는 일(URL → 메서드 등록)을 C에서 직접 표로 만드는 것과 같다. Java의 `Map<Integer, Handler>`나 인터페이스 구현체 목록과 비슷하다
  - M5-5에서는 같은 원리로 "입력 소스(파일 / 소켓)"를 함수 포인터 구조체로 바꿔 끼운다 (Java의 interface 역할)
- 메시지가 3개뿐인 지금 switch를 고른 이유 → **지금은 단순한 게 낫다.** 3개면 switch가 짧고 읽기 쉽다. 함수 포인터 표는 handler 함수 3개를 따로 만들고 타입을 맞추는 준비가 더 들어서, 지금 규모에서는 복잡함만 늘어난다. 메시지가 많아지거나 런타임에 바꿔 끼워야 할 때(M5-5) 바꾼다 → 필요해질 때 바꾸는 것도 설계 판단이다 (YAGNI: You Aren't Gonna Need It)

## 7. 테스트 (이슈 완료 조건)

| 완료 조건 | 테스트 |
| --- | --- |
| 알 수 없는 ID | `test_dispatch_unknown_id_ignored` (`0x7FF` → IGNORED, 아무 상태도 안 바뀜) |
| 짧은 DLC | `test_dispatch_short_dlc_keeps_previous` (정상 100.00 km/h 뒤에 DLC 1 → ERROR, 차속과 `rx_ms` 그대로) |
| 정상 프레임 | `test_dispatch_speed`, `test_dispatch_door`, `test_dispatch_ign` (각각 OK, **해당 상태만** 갱신, 다른 상태는 그대로) |
| (추가) 초기 상태 | `test_init` (전부 안 받음, 기어 `GEAR_INVALID`) |
| (추가) 우리 메시지 | `test_dispatch_own_bcm_status_ignored` (`0x400` → IGNORED) |
| (추가) 잘못된 프레임 | `test_dispatch_invalid_frame` (DLC 9 → ERROR), `test_dispatch_null` (NULL → ERROR) |
| (추가) 신호 무효 | `test_dispatch_invalid_signal_is_ok` (`FF FF` → OK, `speed.valid = false`) |
| (추가) 통계 | `test_dispatch_stats` (OK 2, IGNORED 1, ERROR 1) |
| (추가) 시나리오 | `test_dispatch_scenario` (프레임 6개가 쌓여 최종 상태 완성) |

- 결과 → `[test_dispatcher]` **48 passed**, 0 failed (전체 테스트: bit_utils 23 + can_frame 13 + can_signal 225 + dispatcher 48 + signals 125)
- `test_dispatch_scenario`가 보여주는 것 (M4와 연결) → 실제 운전처럼 **시간 순서대로 프레임이 하나씩 들어오면서** 차량 상태가 쌓여 가는 과정을 확인한다

```
  0ms  시동 ON, P          →  ign = ON, P
 50ms  문 전부 닫힘          →  doors = 전부 false
100ms  D단                 →  gear = D
150ms  상관없는 0x555       →  무시
200ms  30.00 km/h          →  speed = 3000
300ms  운전석 열림!          →  doors.fl = true
최종: "D단, 30 km/h로 달리는데 운전석 문이 열림"
```

  - 이 최종 상태는 M4 상태 머신에 들어갈 **입력 그대로**다. M4에서는 이 상태를 보고 **WARNING**(경고음 ON)으로 바꾸는 로직을 만든다 → 디스패처(M2)가 "해석"을 끝내 두었기 때문에, 상태 머신(M4)은 "판단"만 하면 된다
  - M3에서 로그 파일로 만들 시나리오 `02_door_open_while_driving.log`가 바로 이 흐름이다
