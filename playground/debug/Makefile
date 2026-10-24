# M0-6 실험 빌드
#   make        → 일반 빌드(build/) + 검사 도구 빌드(build-san/) 둘 다
#   make warn   → 01_warnings.c를 경고 옵션별로 컴파일해서 비교
#   make clean
CC       := gcc
CFLAGS   := -std=c11 -Wall -Wextra -g -O0
SANFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

SRCS := $(wildcard *.c)
NORMAL := $(patsubst %.c,build/%,$(SRCS))
SAN    := $(patsubst %.c,build-san/%,$(SRCS))

.PHONY: all warn clean

all: $(NORMAL) $(SAN)

build/%: %.c | build
	$(CC) $(CFLAGS) $< -o $@

build-san/%: %.c | build-san
	$(CC) $(CFLAGS) $(SANFLAGS) $< -o $@

build build-san:
	mkdir -p $@

warn:
	@echo "===== (1) 경고 옵션 없음"
	@$(CC) -std=c11 -c 01_warnings.c -o /dev/null
	@echo "===== (2) -Wall -Wextra"
	@$(CC) -std=c11 -Wall -Wextra -c 01_warnings.c -o /dev/null
	@echo "===== (3) -Wall -Wextra -Wconversion -Wshadow"
	@$(CC) -std=c11 -Wall -Wextra -Wconversion -Wshadow -c 01_warnings.c -o /dev/null

clean:
	rm -rf build build-san
