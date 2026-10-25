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

# make test: tests/test_*.c 하나당 테스트 실행 파일 하나 (main.c는 빼고 링크)
LIB_OBJS   := $(filter-out $(BUILD)/main.o,$(OBJS))
TEST_BUILD := $(BUILD)/tests
TEST_SRCS  := $(wildcard tests/test_*.c)
TEST_BINS  := $(patsubst tests/%.c,$(TEST_BUILD)/%,$(TEST_SRCS))

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

$(BUILD) $(DEBUG_BUILD) $(TEST_BUILD):
	mkdir -p $@

# 모든 테스트를 실행하고, 하나라도 실패하면 make도 실패(종료 코드 1)
test: $(TEST_BINS)
	@fail=0; \
	for t in $(TEST_BINS); do \
		./$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "❌ 테스트 실패"; else echo "✅ 모든 테스트 통과"; fi; \
	exit $$fail

# 테스트 빌드: tests/test_xxx.c + main.o를 뺀 src의 .o들 → build/tests/test_xxx
$(TEST_BUILD)/%: tests/%.c $(LIB_OBJS) | $(TEST_BUILD)
	$(CC) $(CFLAGS) -Itests $^ -o $@

clean:
	rm -rf $(BUILD)
