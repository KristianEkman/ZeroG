CC = gcc
CFLAGS_COMMON = -Wall -Wextra -std=c99
BUILD ?= release

ifeq ($(BUILD),release)
	CFLAGS_OPT = -O3 -DNDEBUG -march=native -flto
	LDFLAGS += -flto
else ifeq ($(BUILD),debug)
	CFLAGS_OPT = -O0 -g3
else
	$(error Unsupported BUILD='$(BUILD)'. Use BUILD=release or BUILD=debug)
endif

SRC_DIR = src
TEST_DIR = test
BUILD_DIR = builds
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_OPT) -I$(SRC_DIR) -I$(SRC_DIR)/movegen -I$(SRC_DIR)/hashing -I$(SRC_DIR)/uci -I$(SRC_DIR)/search
TARGET = $(BUILD_DIR)/chessai2027
TEST_TARGET = $(BUILD_DIR)/test_runner
FEN_TEST_TARGET = $(BUILD_DIR)/fen_test_runner
MOVEGEN_TEST_TARGET = $(BUILD_DIR)/movegen_test_runner
PERFT_BENCH_TARGET = $(BUILD_DIR)/perft_bench

PROFILE_BUILD_DIR ?= builds/profile
PROFILE_DEPTH ?= 6
PROFILE_SAMPLE_SECONDS ?= 5

SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/movegen/*.c) \
       $(wildcard $(SRC_DIR)/hashing/*.c) \
       $(wildcard $(SRC_DIR)/uci/*.c) \
       $(wildcard $(SRC_DIR)/search/*.c)

# library sources (everything except main)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_SRCS = $(TEST_DIR)/test_boards.c $(TEST_DIR)/unity.c
FEN_TEST_SRCS = $(TEST_DIR)/fen_test.c $(TEST_DIR)/unity.c
MOVEGEN_TEST_SRCS = $(TEST_DIR)/movegen_tests.c $(TEST_DIR)/unity.c
PERFT_BENCH_SRCS = $(TEST_DIR)/perft_bench.c

.PHONY: all clean test test_fen test_movegen test_all bench_perft release debug profile

all: $(TARGET)

release:
	@$(MAKE) BUILD=release all

debug:
	@$(MAKE) BUILD=debug all

test: test_all

bench_perft: $(PERFT_BENCH_TARGET)
	@echo "Running perft benchmark..."
	@$(PERFT_BENCH_TARGET)

profile:
	$(MAKE) BUILD_DIR=$(PROFILE_BUILD_DIR) CFLAGS_OPT="-O3 -g -DNDEBUG -march=native -flto" $(PROFILE_BUILD_DIR)/perft_bench
	@if command -v valgrind >/dev/null 2>&1 && command -v callgrind_annotate >/dev/null 2>&1; then \
		echo "Running Callgrind on perft_bench (depth $(PROFILE_DEPTH)) ..."; \
		valgrind --tool=callgrind \
		         --callgrind-out-file=$(PROFILE_BUILD_DIR)/callgrind.out \
		         --collect-atstart=yes \
		         $(PROFILE_BUILD_DIR)/perft_bench $(PROFILE_DEPTH); \
		callgrind_annotate --auto=yes --threshold=1 $(PROFILE_BUILD_DIR)/callgrind.out | head -150; \
	elif command -v sample >/dev/null 2>&1; then \
		echo "Running macOS sample on perft_bench (depth $(PROFILE_DEPTH)) ..."; \
		$(PROFILE_BUILD_DIR)/perft_bench $(PROFILE_DEPTH) & \
		pid=$$!; \
		sample $$pid $(PROFILE_SAMPLE_SECONDS) -file $(PROFILE_BUILD_DIR)/sample.txt; \
		wait $$pid; \
		head -150 $(PROFILE_BUILD_DIR)/sample.txt; \
	else \
		echo "No supported profiler found. Install valgrind/callgrind or use a macOS profiler." >&2; \
		exit 1; \
	fi

$(TEST_TARGET): $(LIB_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(TEST_SRCS)

test_fen: $(FEN_TEST_TARGET)
	@echo "Running FEN tests..."
	@$(FEN_TEST_TARGET)

test_movegen: $(MOVEGEN_TEST_TARGET)
	@echo "Running movegen tests..."
	@$(MOVEGEN_TEST_TARGET)

test_all: $(TEST_TARGET) $(FEN_TEST_TARGET) $(MOVEGEN_TEST_TARGET)
	@echo "Running board tests..."
	@$(TEST_TARGET)
	@echo "Running FEN tests..."
	@$(FEN_TEST_TARGET)
	@echo "Running movegen tests..."
	@$(MOVEGEN_TEST_TARGET)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(FEN_TEST_TARGET): $(LIB_SRCS) $(FEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(FEN_TEST_SRCS)

$(MOVEGEN_TEST_TARGET): $(LIB_SRCS) $(MOVEGEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(MOVEGEN_TEST_SRCS)

$(PERFT_BENCH_TARGET): $(LIB_SRCS) $(PERFT_BENCH_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(PERFT_BENCH_SRCS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)