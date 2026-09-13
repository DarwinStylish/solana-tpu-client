CC ?= cc
CXX ?= c++
AR ?= ar
CFLAGS ?= -O3 -Wall -Wextra -Werror -std=c11 -fstack-protector-strong -D_FORTIFY_SOURCE=2
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra -Werror -std=c++17
INCLUDES = -Iinclude

BUILD_DIR = build
LIBRARY = $(BUILD_DIR)/libsolana_ingress.a
LIB_OBJECT = $(BUILD_DIR)/solana_ingress.o
TEST_INGRESS = $(BUILD_DIR)/test_ingress
TEST_CPP = $(BUILD_DIR)/test_cpp_linkage
BENCHMARK = $(BUILD_DIR)/benchmark

.PHONY: all test bench clean

all: $(LIBRARY) $(TEST_INGRESS) $(TEST_CPP) $(BENCHMARK)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(LIB_OBJECT): src/solana_ingress.c include/solana/ingress.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LIBRARY): $(LIB_OBJECT)
	$(AR) rcs $@ $^

$(TEST_INGRESS): tests/test_ingress.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -pthread $< $(LIBRARY) -o $@

$(TEST_CPP): tests/test_cpp_linkage.cpp $(LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

$(BENCHMARK): tests/benchmark.c $(LIBRARY)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $< $(LIBRARY) -o $@

test: $(TEST_INGRESS) $(TEST_CPP)
	@$(TEST_INGRESS)
	@$(TEST_CPP)

bench: $(BENCHMARK)
	@$(BENCHMARK)

clean:
	rm -rf $(BUILD_DIR)
