# 22. CLI 인자 처리 & MVP 실행 파일 (M3-6)

> 코드: `include/cli.h`, `src/cli.c`, `include/version.h`, `src/main.c`, `tests/test_cli.c`, `README.md`
> 실행: `make` 후 아래 명령을 하나씩 실행하고, 결과와 **종료 코드**(`echo $?`)를 적는다
>
> ⚠️ M3-5의 `./build/bcm_sim 파일 debug` 방식은 이제 **안 된다** → `--file`, `--log-level`로 지정

## 1. 실행해 보기

종료 코드는 명령 바로 다음에 `echo $?`로 확인한다.

| 명령 | 결과 | 종료 코드 |
| --- | --- | --- |
| `./build/bcm_sim --help` | 사용법 11줄 (옵션 표 + 예시) | **0** |
| `./build/bcm_sim --version` | `can-bcm-simulator 0.1.0` | **0** |
| `./build/bcm_sim` | `--file이 필요합니다` + `도움말: ./build/bcm_sim --help` | **2** |
| `./build/bcm_sim data/scenarios/01_normal_drive.log` | `알 수 없는 인자: ... (로그 파일은 --file로 지정)` → M3-5 방식은 이제 안 됨 | **2** |
| `./build/bcm_sim --file data/scenarios/02_door_open_while_driving.log` | 정상 실행. INFO 로그 + 결과 요약 (59줄), 4.0초 `FL=1`, 5.0초 `FL=0` | **0** |
| `./build/bcm_sim -f data/scenarios/04_invalid_frames.log -l warn` | WARN 6줄(잘못된 줄 4 + 해석 못 한 프레임 2)만 + 결과 요약 | **0** |
| `./build/bcm_sim -l error --file data/scenarios/01_normal_drive.log` (순서 바꿈) | 로그 0줄, 결과 요약만 → **순서를 바꿔도 된다** | **0** |
| `./build/bcm_sim --file=data/scenarios/01_normal_drive.log` (`=` 형태) | `--file 경로`와 똑같이 정상 실행 (77줄) | **0** |
| `./build/bcm_sim --file nope.log` | 옵션은 맞음 → 실행은 시작 → `ERROR 파일을 열 수 없습니다: nope.log (No such file or directory)` | **1** |
| `./build/bcm_sim -f data/scenarios/01_normal_drive.log -l loud` | `알 수 없는 로그 레벨: loud (debug\|info\|warn\|error\|off)` | **2** |
| `./build/bcm_sim --fast` | `알 수 없는 옵션: --fast` | **2** |

- 종료 코드가 0, 1, 2로 나뉘는 기준 →

| 코드 | 뜻 | 예 |
| --- | --- | --- |
| **0** | 성공 | 정상 실행, `--help`, `--version` |
| **1** | 옵션은 맞는데 **실행하다가** 실패 | 파일이 없음 (`nope.log`) |
| **2** | **명령을 잘못 씀** (실행 시작도 안 함) | `--file` 없음, 모르는 옵션, 틀린 레벨 |

  - 구분 기준: "**사람이 명령을 잘못 쳤나(2)**, 아니면 **명령은 맞는데 일이 안 됐나(1)**"
  - `--help`가 0인 이유: 도움말을 보여 달라고 했고, 보여 줬으니 **성공**이다
  - 2를 "사용법 오류"로 쓰는 건 리눅스 도구들의 관례 (`grep`, `ls` 등도 옵션이 틀리면 2)
- 종료 코드를 나누면 누가 좋은가? (예: `&&`, 스크립트, CI) → **이 프로그램을 부르는 다른 프로그램.** 사람은 화면 글자를 읽으면 되지만, 스크립트나 CI는 글자를 못 읽고 **종료 코드 숫자**만 본다
  - `&&`: `./build/bcm_sim --file a.log && echo 성공` → 0일 때만 다음 명령 실행
  - `make test`도 같은 원리: 테스트 프로그램이 0이 아닌 코드로 끝나면 make가 실패로 처리한다 (M0-7)
  - 나중에 CI(GitHub Actions, M6)에서 시나리오를 돌릴 때, 실패하면 **빨간불**이 켜지는 것도 종료 코드 덕분
  - Java로 치면 `System.exit(2)`, 또는 Spring의 HTTP 상태 코드: **400**(요청이 잘못됨 = 2) vs **500**(서버가 처리하다 실패 = 1) vs **200**(성공 = 0)

## 2. `argc`, `argv` 복습

```
./build/bcm_sim  -f  a.log  -l  warn
   argv[0]      [1]  [2]   [3]  [4]       argc = 5
```

- `argc`의 값 → **5**. 셸이 명령줄을 **띄어쓰기 기준으로 잘라서** 조각 개수를 센다. 프로그램 이름(`./build/bcm_sim`)도 한 조각이라 포함된다
  - `argc` = **arg**ument **c**ount (인자 개수), `argv` = **arg**ument **v**ector (인자 배열)
  - `argv[argc]`는 항상 `NULL` (배열의 끝 표시) → `argv[5] == NULL`
  - `-f`와 `a.log`는 **따로따로** 들어온다. "`-f` 다음 칸이 파일 이름"이라고 짝지어 주는 게 `getopt_long`이 하는 일 (3번)
  - `--file=a.log`처럼 `=`로 붙이면 **한 조각**이다 → `getopt_long`이 `=` 앞뒤를 나눠 준다
  - Java로 치면 `public static void main(String[] args)`의 `args`. 차이: Java의 `args`에는 **프로그램 이름이 없다** (`args[0]`이 바로 `-f`)
- `argv[0]`에는 무엇이 들어 있나? 사용법 메시지에서 왜 `argv[0]`를 찍었나 → **실행할 때 친 프로그램 이름 그대로** 들어 있다 (`./build/bcm_sim`). 사용법에 이름을 직접 적지 않고 `argv[0]`를 찍으면, **어떻게 실행했든 그 이름으로** 안내가 나온다

```
./build/bcm_sim --help        → 사용법: ./build/bcm_sim --file ...
./build/debug/bcm_sim --help  → 사용법: ./build/debug/bcm_sim --file ...   (Sanitizer 빌드)
```

  - 사용자가 안내를 **그대로 복사해서 붙여 넣으면 바로 실행된다**
  - 파일 이름이나 경로가 바뀌어도 코드를 고칠 필요가 없다

## 3. `getopt_long`

```c
static const struct option long_opts[] = {
    { "file",      required_argument, NULL, 'f' },
    { "log-level", required_argument, NULL, 'l' },
    { "help",      no_argument,       NULL, 'h' },
    { "version",   no_argument,       NULL, 'V' },
    { NULL, 0, NULL, 0 }
};
static const char short_opts[] = "f:l:hV";
```

- `getopt_long`이 하는 일 (직접 `argv`를 `strcmp`로 비교하는 것과 비교) → `argv`를 앞에서부터 읽으면서 **옵션을 하나씩 꺼내 주는** 함수. 부를 때마다 다음 옵션의 글자(`'f'`, `'l'`, ...)를 돌려주고, 다 읽으면 `-1`을 돌려준다 → 그래서 `while (... != -1)` + `switch`로 쓴다

```
argv:  [bcm_sim] [-f] [a.log] [--log-level] [warn]
1번째 호출 → 'f'  (optarg = "a.log")
2번째 호출 → 'l'  (optarg = "warn")      ← --log-level도 표를 보고 'l'로 바꿔 줌
3번째 호출 → -1   (끝)
```

  - 직접 `strcmp`로 만들면 처리할 게 많다:

| 경우 | 직접 만들면 | `getopt_long` |
| --- | --- | --- |
| `-f a.log` / `--file a.log` | 둘 다 따로 비교 | 표에서 같은 `'f'`로 |
| `--file=a.log` | `=`를 찾아 잘라야 함 | 알아서 나눠 줌 |
| `-f` 다음에 값이 없음 | `i + 1 < argc`를 매번 확인 | `'?'`로 알려 줌 |
| 순서가 바뀜 | 위치에 기대면 깨짐 | 상관없음 |
| 모르는 옵션 | 직접 걸러야 함 | `'?'`로 알려 줌 |

  - 리눅스 명령어(`ls -la`, `grep --color=auto`)가 대부분 이걸로 만들어져 있어서, 사용자가 **익숙한 방식 그대로** 쓸 수 있다
  - 참고: `getopt_long`은 C 표준이 아니라 **POSIX/GNU 확장**이다. 맥과 리눅스에는 있지만 차량 MCU 같은 환경에는 없을 수 있다 (이 프로그램은 PC에서 도는 시뮬레이터라 괜찮다)
- `"f:l:hV"`에서 `:`의 뜻 → 바로 앞 글자 옵션이 **값을 하나 받는다**는 뜻
  - `f:` → `-f 경로` (값 필요), `l:` → `-l 레벨` (값 필요)
  - `h`, `V` → `:`이 없으니 값 없이 단독 (`-h`, `-V`)
  - 긴 옵션 표의 `required_argument` / `no_argument`가 같은 역할
- `optarg`, `optind`, `optopt`가 각각 무엇인가 → 전부 `<getopt.h>`에 있는 **전역 변수**. `getopt_long`이 부를 때마다 채워 준다

| 변수 | 뜻 | 예 (`-f a.log`를 읽은 직후) |
| --- | --- | --- |
| `optarg` | 지금 옵션의 **값** (문자열 포인터) | `"a.log"` |
| `optind` | **다음에 읽을** `argv` 칸 번호 | `3` |
| `optopt` | 문제가 생긴 옵션 글자 (`'?'`를 돌려줬을 때) | `-x`면 `'x'` |

  - `optarg`는 `argv` 안을 **가리키는 포인터**다 (복사본이 아님). 그래서 `opts->file = optarg;`처럼 포인터만 저장해도 된다. `argv`는 프로그램이 끝날 때까지 살아 있다
  - 루프가 끝난 뒤 `optind < argc`이면 → 옵션이 아닌 인자가 남았다는 뜻 → `알 수 없는 인자` 오류 (M3-5 방식 `./bcm_sim 파일`이 여기서 걸린다)
- 표 마지막의 `{ NULL, 0, NULL, 0 }`은 왜 필요한가 (M0-5 문자열의 `'\0'`과 비슷한 이유) → C 배열은 **자기 길이를 모른다** (함수에 넘기면 포인터가 됨, M0-3). `getopt_long`에는 표의 길이를 따로 안 넘기니까, **"여기가 끝"이라는 표시**가 있어야 어디까지 읽을지 안다 → 이름이 `NULL`인 칸을 만나면 멈춘다
  - 문자열이 `'\0'`으로 끝나는 것, `argv`가 `NULL`로 끝나는 것(2번)과 같은 방식 (**종료 표시, sentinel**)
  - 빠뜨리면 `getopt_long`이 표 뒤의 엉뚱한 메모리까지 읽는다 → UB
- Java(Spring Boot)와 비교: `--server.port=8080`, picocli/`@Option` →

| Java | 이 프로젝트 |
| --- | --- |
| `java -jar app.jar --server.port=8080` | `./bcm_sim --file=a.log` |
| picocli `@Option(names = {"-f", "--file"}, required = true) String file;` | `{ "file", required_argument, NULL, 'f' }` + `"f:"` |
| `@Option(names = {"-h", "--help"}, usageHelp = true)` | `{ "help", no_argument, NULL, 'h' }` |
| 필드에 값을 자동으로 넣어 줌 | `switch`에서 `opts->file = optarg;`로 **직접** 넣음 |
| `required = true`를 어기면 자동 오류 | `opts->file == NULL`을 **직접** 확인 |

  - Java는 어노테이션만 붙이면 프레임워크가 알아서 해 주고, C는 "옵션을 꺼내 주는 것"까지만 도와주고 **나머지는 직접** 한다

## 4. 왜 `cli_parse()`를 `main()`에서 따로 뺐나

- 이유 → **테스트하기 위해서.** `main()`은 프로그램에 하나뿐이고(M3-4), 테스트 파일에도 자기 `main()`이 있어서 `main()`을 테스트에서 부를 수 없다. 해석 부분을 `cli_parse()`라는 보통 함수로 빼면, 테스트에서 `argv`를 만들어 넣고 결과를 `ASSERT`로 확인할 수 있다 (7번)
  - 역할도 나뉜다: `cli_parse` = **"무엇을 하라는 건지" 알아내기**, `main` = **"실제로 하기"**(로그 레벨 설정, 파일 읽기, 결과 출력)
  - Spring으로 치면 Controller에서 요청 파라미터를 검증하는 부분을 따로 빼서, 서버를 띄우지 않고 단위 테스트하는 것
  - `log_read_stream`(M3-3)을 `log_read_file`에서 뺀 것, `log_set_output`(M3-5)을 만든 것과 같은 **"테스트하기 쉽게 설계"**
- `cli_parse`가 프로그램을 끝내지(`exit`) 않고 `CliResult`를 돌려주는 이유 → `cli_parse` 안에서 `exit(2)`를 부르면 **테스트 프로그램까지 같이 끝나 버린다.** `test_errors`에서 오류 하나를 확인하자마자 테스트 전체가 종료되어 나머지 테스트가 안 돈다
  - 그래서 `cli_parse`는 **"어떤 결과인지"만 알려 주고**(`CLI_OK`/`CLI_HELP`/`CLI_VERSION`/`CLI_ERROR`), **끝낼지 말지는 `main`이 정한다**
  - Java로 치면 검증 메서드 안에서 `System.exit()`를 부르지 않고, 결과를 돌려주거나 예외를 던져서 호출한 쪽이 처리하게 하는 것
  - 같은 이유로 오류 메시지도 `FILE *err`로 받는다 → 테스트에서는 `NULL`을 넘겨서 화면을 지저분하게 하지 않는다
- `--help`가 `CLI_ERROR`가 아니라 `CLI_HELP`인 이유 (도움말 출력은 stdout, 오류는 stderr) → 사용자가 **도움말을 보여 달라고 요청한 것**이라 오류가 아니다. 결과가 다르게 처리돼야 한다

| | `--help` (`CLI_HELP`) | 잘못된 옵션 (`CLI_ERROR`) |
| --- | --- | --- |
| 무엇을 찍나 | 사용법 전체 | 무엇이 틀렸는지 + `도움말: ... --help` |
| 어디에 | **stdout** (요청한 결과물) | **stderr** (오류 메시지) |
| 종료 코드 | **0** (성공) | **2** (사용법 오류) |

  - stdout으로 찍어야 `./build/bcm_sim --help | less`, `--help > usage.txt`처럼 **결과로 쓸 수 있다** (M3-5 2번: 결과는 출력, 문제는 오류 쪽)
  - `CLI_ERROR`로 묶었다면 도움말을 볼 때마다 실패(2)로 끝나서, 스크립트가 "실패했다"고 오해한다

## 5. 플랫폼 차이 (`reset_getopt`)

```c
#if defined(__GLIBC__)
    optind = 0;
#else
    optreset = 1;
    optind = 1;
#endif
```

- `getopt_long`의 상태(`optind`)를 왜 되돌려야 하나 (M3-5 5번 "전역 상태") → `getopt_long`은 "다음에 읽을 칸"을 **전역 변수 `optind`**에 기억해서, 다음 호출 때 이어서 읽는다. 한 번 다 읽고 나면 `optind`가 끝에 가 있으므로, 다시 해석하려면 **처음으로 되돌려야** 한다 (7번 `test_parse_twice`)
  - 실제 프로그램은 한 번만 해석해서 필요 없지만, 테스트는 한 프로그램에서 **여러 번** 해석한다
  - 로거의 `s_level`처럼 **전역 상태는 다음 사용까지 남는다** (M3-5 5번). 남이 만든 라이브러리에도 이런 전역 상태가 있다
- 맥과 리눅스에서 되돌리는 방법이 다른 이유 → `getopt_long`을 **만든 곳이 다르기 때문.** C 표준에 없는 함수라 "어떻게 되돌리는지"도 정해진 규칙이 없고, 각자 다르게 만들었다

| | 리눅스 | 맥 |
| --- | --- | --- |
| 라이브러리 | **glibc** (GNU) | **libSystem** (BSD 계열) |
| 되돌리는 방법 | `optind = 0` | `optreset = 1; optind = 1;` |
| `optreset` 변수 | **없음** (쓰면 컴파일 에러) | 있음 |

  - 그래서 `#if defined(__GLIBC__)`로 나눴다. `__GLIBC__`는 glibc를 쓸 때 **자동으로 정의되는 매크로** (`__GNUC__`와 같은 종류, M3-5 4번)
  - `#if`는 **컴파일 전에** 전처리기가 한쪽 코드를 지운다 → 리눅스에서는 `optreset` 줄이 아예 없는 것처럼 컴파일돼서 에러가 안 난다
  - 그래서 이 코드는 맥에서도, 리눅스 컨테이너에서도 테스트를 **둘 다 돌려서** 확인했다. 한쪽에서만 통과하면 다른 쪽에서 깨질 수 있다
  - Java는 JVM이 OS 차이를 감춰 줘서 이런 일이 거의 없다 ("Write once, run anywhere"). C는 OS의 라이브러리를 직접 쓰니 차이가 그대로 드러난다 → **이식성(portability)** 문제
- `opterr = 0`을 한 이유 → `getopt_long`은 기본으로 모르는 옵션을 만나면 **자기가 직접 영어 오류를 stderr에 찍는다.** 우리도 한국어 오류를 찍으니, 그대로 두면 오류가 **두 줄씩** 나온다

```
opterr = 1 (기본):  ./build/bcm_sim: unrecognized option `--fast'     ← getopt가 찍음
                   알 수 없는 옵션: --fast                             ← 우리가 찍음
opterr = 0:        알 수 없는 옵션: --fast                             ← 우리 것만
```

  - 게다가 영어 문구는 맥과 리눅스에서 **조금씩 다르다** (직접 확인: 맥 ``unrecognized option `--fast'``, 리눅스 `unrecognized option '--fast'` → 따옴표 모양이 다름) → 끄고 직접 찍으면 양쪽 메시지가 같아진다
  - 테스트에서 `err = NULL`로 넘기면 아무것도 안 찍히게 하려면, getopt가 몰래 찍지 않도록 꺼야 한다

## 6. 버전과 태그

- `version.h`에 버전을 한 곳에만 둔 이유 → 버전은 **여러 곳에서** 찍힌다 (`--version` 출력, 시작 로그 `can-bcm-simulator 0.1.0 시작`). 곳곳에 `"0.1.0"`을 직접 적으면, 버전을 올릴 때 **하나라도 빠뜨리면** 서로 다른 버전이 찍힌다. 한 곳(`BCM_SIM_VERSION`)에만 두면 **그 한 줄만** 고치면 된다
  - M3-5까지는 `main.c`에 `v0.0.1`이 글자로 박혀 있었다 → 이번에 `version.h`로 옮겼다
  - M1-1의 "매직 넘버 대신 상수(`MSG_ID_VEHICLE_SPEED`)"와 같은 생각
  - Java로 치면 `build.gradle`의 `version = '0.1.0'`을 한 곳에 두고, 코드에서는 그 값을 읽어 쓰는 것
- 버전 번호 `0.1.0`의 세 숫자 뜻 (Semantic Versioning) → **`주.부.수`** (MAJOR.MINOR.PATCH)

| 자리 | 언제 올리나 | 예 |
| --- | --- | --- |
| **주** (MAJOR) | 예전 사용법이 **안 통하게** 바뀔 때 | 옵션 이름을 바꿈 |
| **부** (MINOR) | 예전 사용법은 그대로, **기능이 추가**될 때 | M4에서 BCM 제어 추가 → `0.2.0` |
| **수** (PATCH) | 기능은 그대로, **버그만 고칠** 때 | 파서 버그 수정 → `0.1.1` |

  - 주 번호가 **0**이면 "아직 완성 전(개발 중)이라 언제든 바뀔 수 있다"는 뜻. 그래서 이번처럼 실행 방법이 바뀌어도(M3-5 방식 → `--file`) 주 번호를 안 올렸다
  - 로드맵 계획: M3 = `0.1.0`(첫 실행 가능 버전, MVP), M4 = `0.2.0`, M5 = `0.3.0`, M6 = **`1.0.0`**(완성, 이제부터 사용법을 함부로 안 바꿈)
  - Spring Boot `3.2.1`도 같은 규칙: 3 → 2가 넘어갈 때는 기능 추가, 2.x → 3.x는 크게 바뀜(javax → jakarta)
- git 태그 `v0.1.0`이란? 커밋과 무엇이 다른가 → **특정 커밋에 붙이는 이름표**. 커밋은 `95f9dd4` 같은 외우기 힘든 해시인데, 태그를 붙이면 `v0.1.0`이라는 이름으로 그 커밋을 찾을 수 있다

| | 커밋 | 브랜치 | 태그 |
| --- | --- | --- | --- |
| 정체 | 코드의 **저장 시점** 하나 | 커밋을 가리키는 이름표 | 커밋을 가리키는 이름표 |
| 움직이나? | — | 새 커밋을 하면 **따라 움직인다** | **고정**. 한 번 붙이면 안 움직인다 |
| 용도 | 변경 기록 | 작업 중인 줄기 | "이 시점이 버전 0.1.0이다" 표시 |

  - 비유: 커밋 = 책의 각 쪽, 브랜치 = **책갈피**(읽는 대로 옮김), 태그 = **포스트잇**(중요한 쪽에 붙여 두고 그대로)
  - 나중에 `git checkout v0.1.0`으로 언제든 그 버전 코드로 돌아가 볼 수 있다. GitHub에서는 태그로 **Release** 페이지를 만들 수 있다
  - **main에 머지한 뒤에** 붙인다: 태그는 "출시된 코드"에 붙이는 것이라, PR이 머지되기 전 작업 브랜치에 붙이면 안 된다

```bash
git checkout main && git pull
git tag -a v0.1.0 -m "v0.1.0: 로그 파일 기반 MVP"   # -a: 메시지가 있는 태그
git push origin v0.1.0                              # 태그는 따로 push해야 올라간다
```

## 7. 테스트

- 결과 → `[test_cli]` **25 passed**, 0 failed (맥, 리눅스 컨테이너, Sanitizer, 엄격 경고 모두 통과)

| 테스트 | 확인하는 것 |
| --- | --- |
| `test_long_options` | `--file a.log --log-level debug` |
| `test_short_options` | `-f b.log -l warn` |
| `test_default_level_is_info` | `--log-level`을 안 주면 INFO |
| `test_order_does_not_matter` | `-l error --file d.log` (순서 바꿈) |
| `test_equals_form` | `--file=e.log --log-level=off` |
| `test_help_and_version` | `--help`, `-h` → `CLI_HELP` / `--version` → `CLI_VERSION` |
| `test_errors` | `--file` 없음, 값 없음, 틀린 레벨, 모르는 옵션, M3-5 방식 → 전부 `CLI_ERROR` |
| `test_parse_twice` | 연달아 두 번 해석해도 두 번째가 맞게 나오는지 |

- `char *argv[] = {"bcm_sim", "--file", "a.log", NULL};`로 argv를 직접 만들어 넣을 수 있는 이유 → `argv`는 특별한 게 아니라 그냥 **"문자열 포인터의 배열"**(`char *[]`)이기 때문이다. 셸이 만들어 주든 테스트가 직접 만들든 `cli_parse` 입장에서는 똑같다
  - 실제 실행처럼 **맨 앞은 프로그램 이름**, **맨 끝은 `NULL`**로 맞췄다 (2번: `argv[argc] == NULL`)
  - `ARGC(a)` 매크로 = `sizeof(배열) / sizeof(한 칸) - 1` → 칸 수에서 끝의 `NULL`을 뺀 개수 (M0-3 `sizeof`로 배열 길이 구하기)
  - 이게 가능한 것도 **`cli_parse`를 `main()`에서 따로 뺐기 때문** (4번). `main()` 안에서 직접 해석했다면 테스트할 때마다 프로그램을 실제로 실행해야 한다
  - Java로 치면 `CliParser.parse(new String[]{"--file", "a.log"})`를 단위 테스트에서 부르는 것
- `test_parse_twice`가 확인하는 것 → **`getopt_long`의 상태를 되돌리는 `reset_getopt()`가 제대로 동작하는지.** `getopt_long`은 "몇 번째 칸까지 읽었는지"를 전역 변수 `optind`에 기억한다 (5번). 첫 해석이 끝나면 `optind`가 끝(3)에 가 있어서, 되돌리지 않으면 두 번째 해석은 **처음부터 안 읽고 바로 끝난다** → `--file`을 못 찾아 `CLI_ERROR`
  - 실제 프로그램은 한 번만 해석하니 문제가 안 되지만, **테스트는 한 프로그램 안에서 여러 번** 해석한다 → 테스트를 짜다 보니 드러나는 문제
  - M3-5 5번 "전역 상태는 테스트 사이에 남는다"와 같은 문제 (로거의 `s_level`처럼)
  - 맥과 리눅스는 되돌리는 방법이 달라서, 이 테스트를 **양쪽에서 다 돌려** 확인했다

## 8. M3 회고

- M3에서 만든 것 (M3-1 ~ M3-6 한 줄씩) →

| 이슈 | 만든 것 |
| --- | --- |
| M3-1 (#17) | 입력 로그 형식 정의: candump `-l` 호환 (`docs/log-format.md`) |
| M3-2 (#18) | 한 줄 파서: 글자 하나씩 읽어 시각/인터페이스/ID/데이터를 꺼냄, `ParseResult`로 실패 이유 구분 |
| M3-3 (#19) | 파일 읽기 루프: `fopen`/`fgets`/`fclose`, 너무 긴 줄 처리, `goto cleanup`, 상대 시각 |
| M3-4 (#20) | 시나리오 4개(정상 주행, 주행 중 도어, 신호 끊김, 잘못된 프레임) + 생성기 `make scenarios` |
| M3-5 (#21) | 로거: 레벨(DEBUG~ERROR), 가변 인자 매크로, `format` attribute, 값이 바뀔 때만 INFO |
| M3-6 (#22) | CLI: `getopt_long`(`--file`, `--log-level`, `--help`, `--version`), 종료 코드 0/1/2, README, `v0.1.0` |

- 가장 어려웠던 것 → **포인터로 문자열을 한 글자씩 읽는 파서(M3-2).** `const char **pp`처럼 "포인터의 포인터"로 읽은 위치를 넘기는 것, 그리고 어떤 줄이 와도 배열 밖을 읽지 않게 하는 것이 낯설었다. Java라면 `String.split()`, 정규식으로 몇 줄이면 끝났을 일을 C에서는 직접 한 글자씩 확인해야 했다
  - 그 다음은 `__VA_ARGS__`, `va_list`, `__attribute__` 같은 **C에만 있는 문법**(M3-5)
- 이제 이 프로그램으로 할 수 있는 것 / 아직 못 하는 것 →
  - **할 수 있는 것:** candump 로그 파일을 읽어서 차속/도어/시동/기어 신호를 해석하고, 언제 무엇이 바뀌었는지 시간순으로 보여 준다. 잘못된 줄이나 프레임은 건너뛰고 몇 번째 줄이 왜 틀렸는지 알려 준다. 로그 양을 레벨로 고를 수 있다
  - **아직 못 하는 것:**
    - **판단과 제어가 없다** → 신호를 "읽기만" 하고, "주행 중 문이 열렸으니 경고" 같은 BCM 동작은 안 한다 (**M4**: 상태 머신, 경고/잠금 출력)
    - **신호 끊김(timeout)을 알아채지 못한다** → 시나리오 03에서 0x100이 1초간 안 와도 그냥 마지막 값을 들고 있다 (M4)
    - **실시간이 아니다** → 파일을 다 읽고 끝난다. 실제 버스(vcan)에서 프레임을 받거나 보내지 못한다 (**M5**: SocketCAN)
    - BCM 상태 프레임(0x400)을 **보내지** 않는다 (M4/M5)
