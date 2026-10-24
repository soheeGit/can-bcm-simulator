CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -g -Iinclude

BUILD  := build
TARGET := $(BUILD)/bcm_sim

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# make debug: AddressSanitizer + UndefinedBehaviorSanitizer 포함 빌드 (build/debug/)
SANFLAGS     := -fsanitize=address,undefined -fno-omit-frame-pointer
DEBUG_BUILD  := $(BUILD)/debug
DEBUG_TARGET := $(DEBUG_BUILD)/bcm_sim
DEBUG_OBJS   := $(patsubst src/%.c,$(DEBUG_BUILD)/%.o,$(SRCS))

.PHONY: all debug test clean

all: $(TARGET)

debug: $(DEBUG_TARGET)

# 링크: 모든 .o → 실행 파일
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# 컴파일: src/xxx.c → build/xxx.o
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $(SANFLAGS) $^ -o $@

$(DEBUG_BUILD)/%.o: src/%.c | $(DEBUG_BUILD)
	$(CC) $(CFLAGS) $(SANFLAGS) -c $< -o $@

$(BUILD) $(DEBUG_BUILD):
	mkdir -p $@

test:
	@echo "테스트는 M0-7에서 추가 예정"

clean:
	rm -rf $(BUILD)
