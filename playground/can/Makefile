# M1 실험 빌드: make → 전부 빌드, make run → 전부 실행, make clean
CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -g

SRCS := $(wildcard *.c)
BINS := $(patsubst %.c,build/%,$(SRCS))

.PHONY: all run clean

all: $(BINS)

build/%: %.c | build
	$(CC) $(CFLAGS) $< -o $@

build:
	mkdir -p $@

run: all
	@for b in $(BINS); do echo "===== $$b"; ./$$b; echo; done

clean:
	rm -rf build
