CC = gcc
CFLAGS = -Wall -Wextra -std=c99
SRC_DIR = src
TEST_DIR = test
BUILD_DIR = builds
TARGET = $(BUILD_DIR)/hello
TEST_TARGET = $(BUILD_DIR)/test_runner
SRCS = $(wildcard $(SRC_DIR)/*.c)

# library sources (everything except main)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c, $(SRCS))
TEST_SRCS = $(TEST_DIR)/test_boards.c $(TEST_DIR)/unity.c

.PHONY: all clean test

all: $(TARGET)

test: $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

$(TARGET): $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_TARGET): $(LIB_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(TEST_DIR) -o $@ $(LIB_SRCS) $(TEST_SRCS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
