CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -g -Iinclude

BUILD  := build
TARGET := $(BUILD)/bcm_sim

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

.PHONY: all test clean

all: $(TARGET)

# 링크: 모든 .o → 실행 파일
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# 컴파일: src/xxx.c → build/xxx.o
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

test:
	@echo "테스트는 M0-7에서 추가 예정"

clean:
	rm -rf $(BUILD)
