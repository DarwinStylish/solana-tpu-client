CC ?= cc
CXX ?= c++
AR ?= ar
CFLAGS ?= -O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra -Werror -std=c++17
FUZZ_CC ?= clang
FUZZ_CFLAGS ?= -O1 -g -Wall -Wextra -Werror -std=c11 -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer
INCLUDES = -Iinclude

BUILD_DIR = build
LIBRARY = $(BUILD_DIR)/libsolana_ingress.a
LIB_OBJECT = $(BUILD_DIR)/solana_ingress.o
TEST_INGRESS = $(BUILD_DIR)/test_ingress
TEST_PROPERTIES = $(BUILD_DIR)/test_decode_properties
TEST_CPP = $(BUILD_DIR)/test_cpp_linkage
BENCHMARK = $(BUILD_DIR)/benchmark
FUZZ_DECODE = $(BUILD_DIR)/fuzz_decode

.PHONY: all test bench fuzz-smoke clean

all: $(LIBRARY) $(TEST_INGRESS) $(TEST_PROPERTIES) $(TEST_CPP) $(BENCHMARK)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_OBJECT): src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(LIB_OBJECT)
	$(AR) rcs $@ $^

$(TEST_INGRESS): tests/test_ingress.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -pthread $< $(LIBRARY) -o $@

$(TEST_PROPERTIES): tests/test_decode_properties.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

$(TEST_CPP): tests/test_cpp_linkage.cpp $(LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

$(FUZZ_DECODE): tests/fuzz_decode.c src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(FUZZ_CC) $(CPPFLAGS) $(FUZZ_CFLAGS) $(INCLUDES) tests/fuzz_decode.c src/solana_ingress.c -o $@

$(BENCHMARK): tests/benchmark.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

test: $(TEST_INGRESS) $(TEST_PROPERTIES) $(TEST_CPP)
	@$(TEST_INGRESS)
	@$(TEST_PROPERTIES)
	@$(TEST_CPP)

bench: $(BENCHMARK)
	@$(BENCHMARK)

fuzz-smoke: $(FUZZ_DECODE)
	@$(FUZZ_DECODE) -seed=1 -runs=20000

clean:
	rm -rf $(BUILD_DIR)
