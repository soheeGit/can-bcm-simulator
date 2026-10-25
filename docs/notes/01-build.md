# 01. 빌드 과정 & Makefile (M0-2)

## 1. 컴파일 4단계

| 단계 | 명령 | 결과 파일 | 하는 일 |
| --- | --- | --- | --- |
| ① 전처리 | `gcc -E` | `.i` | `#include`, `#define`을 처리. 헤더 내용을 그대로 붙여 넣는다. **결과는 여전히 C 코드** |
| ② 컴파일 | `gcc -S` | `.s` | C 코드 → 어셈블리(사람이 읽을 수 있는 CPU 명령어) |
| ③ 어셈블 | `gcc -c` | `.o` | 어셈블리 → 기계어. 하지만 다른 파일/라이브러리의 함수는 "빈칸"으로 남아 있음 |
| ④ 링크 | `gcc *.o` | 실행 파일 | 여러 `.o`와 라이브러리(libc)를 합치고 빈칸(함수 주소)을 채워서 실행 파일 완성 |

- `hello.c`(6줄) → `hello.i`는 몇 줄? → 573
- 왜 이렇게 늘어났나? → 전처리기가 `#include <stdio.h>` 한 줄을 **stdio.h 파일 내용 전체로 바꿔치기**했기 때문. stdio.h가 include하는 다른 헤더까지 딸려 들어온다. (아직 컴파일 전이라 기계어가 아니라 C 코드다)
- `hello.s`에서 `printf`를 호출하는 줄:

```
        bl      _printf
```

> `bl` = Branch with Link: 함수 호출 명령 (ARM64). 함수 이름 앞의 `_`는 macOS가 붙이는 규칙.

- `file hello.o` 결과: `Mach-O 64-bit object arm64`
- `.o`를 바로 실행할 수 없는 이유 → 컴파일은 끝났지만 **링크를 안 했기 때문**. `printf`의 실제 코드가 들어 있지 않고 "printf를 부를 것"이라는 표시만 있다.

```
$ nm playground/hello.o
0000000000000000 T _main      ← T: 이 파일 안에 정의 있음
                 U _printf    ← U: Undefined. 링크 때 libc에서 채워야 함
```

## 2. 링크 에러 실험 (`playground/link_demo`)

### 실험 A: `main.c`만 빌드

```
Undefined symbols for architecture arm64:
  "_add", referenced from:
      _main in main-3f62ab.o
ld: symbol(s) not found for architecture arm64
clang: error: linker command failed with exit code 1 (use -v to see invocation)
```

- 컴파일 단계는 통과했나? → 컴파일은 통과
- 어느 단계에서 실패했나? 왜? → 링크에서 실패. add의 **정의(구현)**를 어디서도 못 찾았기 때문
- 에러를 낸 주체: `ld` (링커). 컴파일러가 아니다.

### 실험 B: 따로 컴파일(`-c`) 후 링크

- 결과: `1 + 2 = 3` 정상 출력
- `nm`으로 본 각 `.o`:

```
$ nm main.o            $ nm math_util.o
     U _add                 T _add
     T _main
     U _printf
```

- `main.o`에서 비어 있던(`U`) `_add`를 `math_util.o`의 `T _add`가 채워줬다 → 링크 성공
- 완성된 `demo`를 `nm`으로 보면 `T _add`, `T _main`은 들어 있지만 `U _printf`는 그대로 남아 있다
  - `printf`는 **동적 링크**: 링커는 "printf는 `libSystem.B.dylib`에 있다"는 메모만 남기고(`otool -L demo`로 확인), 실제 연결은 **실행하는 순간** 동적 로더(macOS `dyld`, Linux `ld-linux`)가 한다
  - `add`는 **정적 링크**: `math_util.o`의 코드가 `demo` 파일 안에 직접 복사된다

### 실험 C (선택): `#include "math_util.h"` 제거

```
main.c:4:28: error: call to undeclared function 'add'; ISO C99 and later do not support implicit function declarations [-Wimplicit-function-declaration]
    4 |     printf("1 + 2 = %d\n", add(1, 2));
      |                            ^
1 error generated.
```

- 실험 A와 무엇이 다른가? → 실험 A는 **링크**에서 실패(정의 없음), 실험 C는 **컴파일**에서 실패(선언 없음). 컴파일러는 파일 하나씩만 보기 때문에, 그 파일 안에서 `add`가 어떻게 생긴 함수인지(선언) 모르면 바로 에러를 낸다.

### 정리: 선언 vs 정의

| | 선언 (declaration) | 정의 (definition) |
| --- | --- | --- |
| 예시 | `int add(int a, int b);` | `int add(int a, int b) { return a + b; }` |
| 의미 | "이런 모양의 함수가 어딘가 있다" | "실제 동작은 이것이다" |
| 보통 위치 | 헤더 (`.h`) | 소스 (`.c`) |
| 없으면 어느 단계에서 에러? | **컴파일** (`undeclared function`) | **링크** (`Undefined symbols` / `undefined reference`) |
| Java로 치면 | interface의 메서드 시그니처 | 구현 클래스의 메서드 본문 |

## 3. Makefile

### 처음 알게 된 문법

| 문법 | 뜻 |
| --- | --- |
| `목표: 재료` + Tab `명령` | "목표 파일을 만들려면 재료가 필요하고, 이 명령으로 만든다" |
| `$@` | 목표 이름 (예: `build/main.o`) |
| `$<` | 첫 번째 재료 (예: `src/main.c`) |
| `$^` | 재료 전부 (예: 모든 `.o`) |
| `%` 패턴 | 같은 이름끼리 짝짓기. `build/%.o: src/%.c` → `src/main.c`로 `build/main.o`를 만든다 |
| `.PHONY` | `all`, `test`, `clean`은 파일이 아니라 명령 이름이라는 표시 (같은 이름의 파일이 있어도 항상 실행) |

### Tab 대신 스페이스를 쓰면?

```
Makefile:29: *** missing separator.  Stop.
```

- 명령 줄 앞은 반드시 **Tab**. 에디터가 Tab을 스페이스로 자동 변환하지 않는지 주의.

### 증분 빌드 관찰

- `make` 두 번 연속 실행 시 두 번째 결과 → `make: Nothing to be done for 'all'.` (바뀐 게 없으니 아무것도 안 함)
- `touch src/main.c` 후 `make` → 다시 컴파일된 파일: `build/main.o` → 그다음 `build/bcm_sim` 다시 링크
- Make는 무엇을 기준으로 재빌드 여부를 판단하나? → **파일 수정 시각**. 재료(`.c`)가 목표(`.o`)보다 새로우면 다시 만든다. (`touch`는 내용은 그대로 두고 수정 시각만 바꾸는 명령)

### macOS vs 컨테이너

- 맥 `make clean && make`: ✅ (Apple clang)
- 컨테이너 `make clean && make`: ✅ (GNU gcc 13.3) → 같은 Makefile이 양쪽에서 동작

## 4. Gradle과 비교

| | Gradle (Java) | Make (C) |
| --- | --- | --- |
| 설정 파일 | `build.gradle` | `Makefile` |
| 빌드 결과물 위치 | `build/` | `build/` (Makefile에서 직접 지정) |
| 컴파일 결과 | `.class` | `.o` |
| 최종 산출물 | `.jar` | 실행 파일 (`build/bcm_sim`) |
| 의존 라이브러리 | 자동 다운로드 (Maven Central) | 자동 다운로드 없음. 시스템에 설치된 라이브러리를 직접 링크 |
| 빌드 규칙 | 플러그인이 알아서 정함 | 규칙을 전부 내가 작성 |
