CC       = gcc
CFLAGS   = -O3 -Wall -Werror -std=c11
CORE_INC = ../../engine/private/include
INCLUDES = -I$(CORE_INC)

BUILD_DIR = build

TEST_SOLANA = $(BUILD_DIR)/test_solana_tpu

.PHONY: all test clean

all: $(TEST_SOLANA)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_SOLANA): tests/test_solana_tpu.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

test: $(TEST_SOLANA)
	@$(TEST_SOLANA)

clean:
	rm -rf $(BUILD_DIR)
