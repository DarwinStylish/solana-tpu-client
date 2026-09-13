CC ?= cc
AR ?= ar
CFLAGS ?= -O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2
CPPFLAGS ?=
INCLUDES = -Iinclude

BUILD_DIR = build
LIBRARY = $(BUILD_DIR)/libsolana_ingress.a
LIB_OBJECT = $(BUILD_DIR)/solana_ingress.o
TEST_INGRESS = $(BUILD_DIR)/test_ingress
BENCHMARK = $(BUILD_DIR)/benchmark

.PHONY: all test bench clean

all: $(LIBRARY) $(TEST_INGRESS) $(BENCHMARK)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_OBJECT): src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(LIB_OBJECT)
	$(AR) rcs $@ $^

$(TEST_INGRESS): tests/test_ingress.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -pthread $< $(LIBRARY) -o $@

$(BENCHMARK): tests/benchmark.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

test: $(TEST_INGRESS)
	@$(TEST_INGRESS)

bench: $(BENCHMARK)
	@$(BENCHMARK)

clean:
	rm -rf $(BUILD_DIR)
