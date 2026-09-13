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
DELIVERY_LIBRARY = $(BUILD_DIR)/libsolana_delivery.a
DELIVERY_OBJECT = $(BUILD_DIR)/solana_delivery.o
TEST_INGRESS = $(BUILD_DIR)/test_ingress
TEST_PROPERTIES = $(BUILD_DIR)/test_decode_properties
TEST_CPP = $(BUILD_DIR)/test_cpp_linkage
TEST_DELIVERY_ABI = $(BUILD_DIR)/test_delivery_abi
TEST_DELIVERY_CPP = $(BUILD_DIR)/test_delivery_cpp
TEST_DELIVERY_TOPOLOGY = $(BUILD_DIR)/test_delivery_topology
TEST_DELIVERY_CLIENT = $(BUILD_DIR)/test_delivery_client
BENCHMARK = $(BUILD_DIR)/benchmark
FUZZ_DECODE = $(BUILD_DIR)/fuzz_decode
FUZZ_TOPOLOGY = $(BUILD_DIR)/fuzz_topology

.PHONY: all test bench fuzz-smoke clean

all: $(LIBRARY) $(DELIVERY_LIBRARY) $(TEST_INGRESS) $(TEST_PROPERTIES) $(TEST_CPP) $(TEST_DELIVERY_ABI) $(TEST_DELIVERY_CPP) $(TEST_DELIVERY_TOPOLOGY) $(TEST_DELIVERY_CLIENT) $(BENCHMARK)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_OBJECT): src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(LIB_OBJECT)
	$(AR) rcs $@ $^

$(DELIVERY_OBJECT): src/solana_delivery.c src/solana_delivery_internal.h include/solana/delivery.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(DELIVERY_LIBRARY): $(DELIVERY_OBJECT)
	$(AR) rcs $@ $^

$(TEST_INGRESS): tests/test_ingress.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -pthread $< $(LIBRARY) -o $@

$(TEST_PROPERTIES): tests/test_decode_properties.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

$(TEST_CPP): tests/test_cpp_linkage.cpp $(LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

$(TEST_DELIVERY_ABI): tests/test_delivery_abi.c include/solana/delivery.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< -o $@

$(TEST_DELIVERY_CPP): tests/test_delivery_cpp.cpp $(DELIVERY_LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $< $(DELIVERY_LIBRARY) -o $@

$(TEST_DELIVERY_TOPOLOGY): tests/test_delivery_topology.c $(DELIVERY_LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(DELIVERY_LIBRARY) -o $@

$(TEST_DELIVERY_CLIENT): tests/test_delivery_client.c $(DELIVERY_LIBRARY) src/solana_delivery_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -Isrc $< $(DELIVERY_LIBRARY) -o $@

$(FUZZ_DECODE): tests/fuzz_decode.c src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(FUZZ_CC) $(CPPFLAGS) $(FUZZ_CFLAGS) $(INCLUDES) tests/fuzz_decode.c src/solana_ingress.c -o $@

$(FUZZ_TOPOLOGY): tests/fuzz_topology.c src/solana_delivery.c include/solana/delivery.h | $(BUILD_DIR)
	$(FUZZ_CC) $(CPPFLAGS) $(FUZZ_CFLAGS) $(INCLUDES) tests/fuzz_topology.c src/solana_delivery.c -o $@

$(BENCHMARK): tests/benchmark.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

test: $(TEST_INGRESS) $(TEST_PROPERTIES) $(TEST_CPP) $(TEST_DELIVERY_ABI) $(TEST_DELIVERY_CPP) $(TEST_DELIVERY_TOPOLOGY) $(TEST_DELIVERY_CLIENT)
	@$(TEST_INGRESS)
	@$(TEST_PROPERTIES)
	@$(TEST_CPP)
	@$(TEST_DELIVERY_ABI)
	@$(TEST_DELIVERY_CPP)
	@$(TEST_DELIVERY_TOPOLOGY)
	@$(TEST_DELIVERY_CLIENT)

bench: $(BENCHMARK)
	@$(BENCHMARK)

fuzz-smoke: $(FUZZ_DECODE) $(FUZZ_TOPOLOGY)
	@$(FUZZ_DECODE) -seed=1 -runs=20000 -max_len=65
	@$(FUZZ_TOPOLOGY) -seed=1 -runs=20000 -max_len=512

clean:
	rm -rf $(BUILD_DIR)
