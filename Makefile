CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = builds
TARGET = $(BUILD_DIR)/hello
TEST_TARGET = $(BUILD_DIR)/test_runner
FEN_TEST_TARGET = $(BUILD_DIR)/fen_test_runner
MOVEGEN_TEST_TARGET = $(BUILD_DIR)/movegen_test_runner
SRCS = $(wildcard $(SRC_DIR)/*.c)

# library sources (everything except main)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_SRCS = $(TEST_DIR)/test_boards.c $(TEST_DIR)/unity.c
FEN_TEST_SRCS = $(TEST_DIR)/fen_test.c $(TEST_DIR)/unity.c
MOVEGEN_TEST_SRCS = $(TEST_DIR)/movegen_tests.c $(TEST_DIR)/unity.c

.PHONY: all clean test test_fen test_movegen test_all

all: $(TARGET)

test: $(TEST_TARGET)
	@echo "Running board tests..."
	@$(TEST_TARGET)

test_fen: $(FEN_TEST_TARGET)
	@echo "Running FEN tests..."
	@$(FEN_TEST_TARGET)

test_movegen: $(MOVEGEN_TEST_TARGET)
	@echo "Running movegen tests..."
	@$(MOVEGEN_TEST_TARGET)

test_all: test test_fen test_movegen

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(LIB_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(TEST_SRCS)

$(FEN_TEST_TARGET): $(LIB_SRCS) $(FEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(FEN_TEST_SRCS)

$(MOVEGEN_TEST_TARGET): $(LIB_SRCS) $(MOVEGEN_TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(MOVEGEN_TEST_SRCS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)