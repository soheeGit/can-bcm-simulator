CC     := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wswitch-enum -Werror -g -Iinclude

# 헤더 의존성 자동 추적 (M4-5): 컴파일할 때 "이 .c가 #include한 .h 목록"을 .d 파일로 같이 만든다.
# 그러면 헤더(예: dispatcher.h의 구조체)가 바뀌었을 때 그 헤더를 쓰는 .c가 전부 다시 컴파일된다.
# -MMD: .d 파일 생성 (시스템 헤더 제외)  -MP: 헤더를 지워도 make가 에러 나지 않게
DEPFLAGS := -MMD -MP

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

# make scenarios: tools/gen_scenarios.c로 data/scenarios/*.log를 다시 만든다 (M3-4)
GEN_SCENARIOS := $(BUILD)/gen_scenarios

.PHONY: all debug test clean scenarios

all: $(TARGET)

debug: $(DEBUG_TARGET)

# 링크: 모든 .o → 실행 파일
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# 컴파일: src/xxx.c → build/xxx.o
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $(SANFLAGS) $^ -o $@

$(DEBUG_BUILD)/%.o: src/%.c | $(DEBUG_BUILD)
	$(CC) $(CFLAGS) $(SANFLAGS) $(DEPFLAGS) -c $< -o $@

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
# ($^에는 .d가 알려 준 헤더도 들어 있어서, 컴파일러에는 .c와 .o만 골라 넘긴다)
$(TEST_BUILD)/%: tests/%.c $(LIB_OBJS) | $(TEST_BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) -Itests $(filter %.c %.o,$^) -o $@

scenarios: $(GEN_SCENARIOS)
	./$(GEN_SCENARIOS) data/scenarios

$(GEN_SCENARIOS): tools/gen_scenarios.c $(LIB_OBJS) | $(BUILD)
	$(CC) $(CFLAGS) $(DEPFLAGS) $(filter %.c %.o,$^) -o $@

clean:
	rm -rf $(BUILD)

# 컴파일러가 만든 의존성 파일을 읽는다 (처음 빌드할 때는 없으니 - 로 "없어도 괜찮음")
-include $(OBJS:.o=.d) $(DEBUG_OBJS:.o=.d) $(TEST_BINS:=.d) $(GEN_SCENARIOS).d
