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
CFLAGS = $(CFLAGS_COMMON) $(CFLAGS_OPT) -I$(SRC_DIR) -I$(SRC_DIR)/movegen -I$(SRC_DIR)/hashing -I$(SRC_DIR)/uci -I$(SRC_DIR)/search -I$(SRC_DIR)/eval -I$(SRC_DIR)/nn
TARGET = $(BUILD_DIR)/chessai2027
TEST_TARGET = $(BUILD_DIR)/test_runner
FEN_TEST_TARGET = $(BUILD_DIR)/fen_test_runner
MOVEGEN_TEST_TARGET = $(BUILD_DIR)/movegen_test_runner
EVAL_TEST_TARGET = $(BUILD_DIR)/eval_test_runner
SEARCH_TEST_TARGET = $(BUILD_DIR)/search_test_runner
NN_TEST_TARGET = $(BUILD_DIR)/nn_test_runner
PERFT_BENCH_TARGET = $(BUILD_DIR)/perft_bench
SEARCH_BENCH_TARGET = $(BUILD_DIR)/search_bench
NN_BENCH_TARGET = $(BUILD_DIR)/nn_bench

PROFILE_BUILD_DIR ?= builds/profile
PROFILE_DEPTH ?= 6
PROFILE_SAMPLE_SECONDS ?= 5

SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/movegen/*.c) \
       $(wildcard $(SRC_DIR)/hashing/*.c) \
       $(wildcard $(SRC_DIR)/uci/*.c) \
       $(wildcard $(SRC_DIR)/search/*.c) \
       $(wildcard $(SRC_DIR)/eval/*.c) \
       $(wildcard $(SRC_DIR)/nn/*.c)

# library sources (everything except main)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_SRCS = $(TEST_DIR)/test_boards.c $(TEST_DIR)/unity.c
FEN_TEST_SRCS = $(TEST_DIR)/fen_test.c $(TEST_DIR)/unity.c
MOVEGEN_TEST_SRCS = $(wildcard $(TEST_DIR)/movegen_tests/*.c) $(TEST_DIR)/unity.c
EVAL_TEST_SRCS = $(TEST_DIR)/eval_test.c $(TEST_DIR)/unity.c
SEARCH_TEST_SRCS = $(TEST_DIR)/search_test.c $(TEST_DIR)/unity.c
NN_TEST_SRCS = $(TEST_DIR)/nn_test.c $(TEST_DIR)/unity.c
PERFT_BENCH_SRCS = $(TEST_DIR)/perft_bench.c
SEARCH_BENCH_SRCS = $(TEST_DIR)/search_bench.c
NN_BENCH_SRCS = $(TEST_DIR)/nn_bench.c

.PHONY: all clean test test_fen test_movegen test_eval test_search test_nn test_all bench_perft bench_search bench_nn release debug profile profile_search profile_nn

all: $(TARGET)

release:
	@$(MAKE) BUILD=release all

debug:
	@$(MAKE) BUILD=debug all

test: test_all

bench_perft: $(PERFT_BENCH_TARGET)
	@echo "Running perft benchmark..."
	@$(PERFT_BENCH_TARGET)

bench_search: $(SEARCH_BENCH_TARGET)
	@echo "Running search benchmark..."
	@$(SEARCH_BENCH_TARGET)

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

profile_search:
	$(MAKE) BUILD_DIR=$(PROFILE_BUILD_DIR) CFLAGS_OPT="-O3 -g -DNDEBUG -march=native -flto" $(PROFILE_BUILD_DIR)/search_bench
	@if command -v valgrind >/dev/null 2>&1 && command -v callgrind_annotate >/dev/null 2>&1; then \
		echo "Running Callgrind on search_bench (depth $(PROFILE_DEPTH)) ..."; \
		valgrind --tool=callgrind \
		         --callgrind-out-file=$(PROFILE_BUILD_DIR)/callgrind.out \
		         --collect-atstart=yes \
		         $(PROFILE_BUILD_DIR)/search_bench $(PROFILE_DEPTH); \
		callgrind_annotate --auto=yes --threshold=1 $(PROFILE_BUILD_DIR)/callgrind.out | head -150; \
	elif command -v sample >/dev/null 2>&1; then \
		echo "Running macOS sample on search_bench (depth $(PROFILE_DEPTH)) ..."; \
		$(PROFILE_BUILD_DIR)/search_bench $(PROFILE_DEPTH) & \
		pid=$$!; \
		sample $$pid $(PROFILE_SAMPLE_SECONDS) -file $(PROFILE_BUILD_DIR)/sample.txt; \
		wait $$pid; \
		head -150 $(PROFILE_BUILD_DIR)/sample.txt; \
	else \
		echo "No supported profiler found. Install valgrind/callgrind or use a macOS profiler." >&2; \
		exit 1; \
	fi

bench_nn: $(NN_BENCH_TARGET)
	@echo "Running neural network benchmark..."
	@$(NN_BENCH_TARGET)

PROFILE_NN_ITERATIONS ?= 200000
profile_nn:
	$(MAKE) BUILD_DIR=$(PROFILE_BUILD_DIR) CFLAGS_OPT="-O3 -g -DNDEBUG -march=native -flto" $(PROFILE_BUILD_DIR)/nn_bench
	@if command -v valgrind >/dev/null 2>&1 && command -v callgrind_annotate >/dev/null 2>&1; then \
		echo "Running Callgrind on nn_bench ..."; \
		valgrind --tool=callgrind \
		         --callgrind-out-file=$(PROFILE_BUILD_DIR)/callgrind.out \
		         --collect-atstart=yes \
		         $(PROFILE_BUILD_DIR)/nn_bench $(PROFILE_NN_ITERATIONS); \
		callgrind_annotate --auto=yes --threshold=1 $(PROFILE_BUILD_DIR)/callgrind.out | head -150; \
	elif command -v sample >/dev/null 2>&1; then \
		echo "Running macOS sample on nn_bench ..."; \
		$(PROFILE_BUILD_DIR)/nn_bench $(PROFILE_NN_ITERATIONS) & \
		pid=$$!; \
		sample $$pid $(PROFILE_SAMPLE_SECONDS) -file $(PROFILE_BUILD_DIR)/sample.txt; \
		wait $$pid; \
		head -150 $(PROFILE_BUILD_DIR)/sample.txt; \
	else \
		echo "No supported profiler found. Install valgrind/callgrind or use a macOS profiler." >&2; \
		exit 1; \
	fi

$(TEST_TARGET): $(LIB_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(TEST_SRCS) $(LDFLAGS) -lm

test_fen: $(FEN_TEST_TARGET)
	@echo "Running FEN tests..."
	@$(FEN_TEST_TARGET)

test_movegen: $(MOVEGEN_TEST_TARGET)
	@echo "Running movegen tests..."
	@$(MOVEGEN_TEST_TARGET)

test_eval: $(EVAL_TEST_TARGET)
	@echo "Running evaluation tests..."
	@$(EVAL_TEST_TARGET)

test_search: $(SEARCH_TEST_TARGET)
	@echo "Running search tests..."
	@$(SEARCH_TEST_TARGET)

test_nn: $(NN_TEST_TARGET)
	@echo "Running neural network tests..."
	@$(NN_TEST_TARGET)

test_all: $(TEST_TARGET) $(FEN_TEST_TARGET) $(MOVEGEN_TEST_TARGET) $(EVAL_TEST_TARGET) $(SEARCH_TEST_TARGET) $(NN_TEST_TARGET)
	@echo "Running board tests..."
	@$(TEST_TARGET)
	@echo "Running FEN tests..."
	@$(FEN_TEST_TARGET)
	@echo "Running movegen tests..."
	@$(MOVEGEN_TEST_TARGET)
	@echo "Running evaluation tests..."
	@$(EVAL_TEST_TARGET)
	@echo "Running search tests..."
	@$(SEARCH_TEST_TARGET)
	@echo "Running neural network tests..."
	@$(NN_TEST_TARGET)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lm

$(FEN_TEST_TARGET): $(LIB_SRCS) $(FEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(FEN_TEST_SRCS) $(LDFLAGS) -lm

$(MOVEGEN_TEST_TARGET): $(LIB_SRCS) $(MOVEGEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -I$(TEST_DIR)/movegen_tests -o $@ $(LIB_SRCS) $(MOVEGEN_TEST_SRCS) $(LDFLAGS) -lm

$(EVAL_TEST_TARGET): $(LIB_SRCS) $(EVAL_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(EVAL_TEST_SRCS) $(LDFLAGS) -lm

$(SEARCH_TEST_TARGET): $(LIB_SRCS) $(SEARCH_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(SEARCH_TEST_SRCS) $(LDFLAGS) -lm

$(NN_TEST_TARGET): $(LIB_SRCS) $(NN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(NN_TEST_SRCS) $(LDFLAGS) -lm

$(PERFT_BENCH_TARGET): $(LIB_SRCS) $(PERFT_BENCH_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(PERFT_BENCH_SRCS) $(LDFLAGS) -lm

$(SEARCH_BENCH_TARGET): $(LIB_SRCS) $(SEARCH_BENCH_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(SEARCH_BENCH_SRCS) $(LDFLAGS) -lm

$(NN_BENCH_TARGET): $(LIB_SRCS) $(NN_BENCH_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(NN_BENCH_SRCS) $(LDFLAGS) -lm

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)