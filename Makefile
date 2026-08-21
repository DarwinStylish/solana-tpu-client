CC       = gcc
CFLAGS = -O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2
CORE_INC = ../../engine/private/include
INCLUDES = -I$(CORE_INC)

BUILD_DIR = build

TEST_SOLANA = $(BUILD_DIR)/test_solana_tpu
BENCHMARK   = $(BUILD_DIR)/benchmark

.PHONY: all test bench clean

all: $(TEST_SOLANA) $(BENCHMARK)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_SOLANA): tests/test_solana_tpu.c src/solana_adapter.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -pthread $^ -o $@

$(BENCHMARK): tests/benchmark.c src/solana_adapter.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -pthread $^ -o $@

test: $(TEST_SOLANA)
	@$(TEST_SOLANA)

bench: $(BENCHMARK)
	@$(BENCHMARK)

clean:
	rm -rf $(BUILD_DIR)
