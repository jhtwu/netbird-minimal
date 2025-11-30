# Makefile for NetBird minimal C client
#
# Author: Claude
# Date: 2025-11-30

CC = gcc
CFLAGS = -Wall -Wextra -I./include -g -O2
LDFLAGS = -lcjson

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = test
BUILD_DIR = build

# Source files (exclude main.c)
SRCS = $(filter-out $(SRC_DIR)/main.c, $(wildcard $(SRC_DIR)/*.c))
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Main binary
MAIN_BIN = $(BUILD_DIR)/netbird-client

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%)

# Targets
.PHONY: all clean test dirs

all: dirs $(MAIN_BIN) $(TEST_BINS)

dirs:
	@mkdir -p $(BUILD_DIR)

# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build main binary
$(MAIN_BIN): $(SRC_DIR)/main.c $(OBJS)
	@echo "Building netbird-client..."
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Build test binaries
$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(OBJS)
	@echo "Building test $@..."
	$(CC) $(CFLAGS) $< $(OBJS) $(LDFLAGS) -o $@

# Run tests
test: all
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo ""; \
		echo "=== Running $$test ==="; \
		sudo $$test || exit 1; \
	done
	@echo ""
	@echo "All tests passed!"

# Clean
clean:
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)

# Install (TODO: implement in Phase 5)
install:
	@echo "Install not yet implemented"

# Help
help:
	@echo "NetBird Minimal C Client - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build netbird-client and all test binaries (default)"
	@echo "  test     - Build and run all tests (requires sudo)"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install netbird-client to /usr/local/bin"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Main binary: build/netbird-client"
	@echo ""
	@echo "Note: Tests require sudo because they create network interfaces"
