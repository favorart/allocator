# Allocator + Pointer: build, test and sanitizer targets.
#   make          - demo and tests
#   make test     - run the tests
#   make sanitize - run the tests under ASan + UBSan
#   make bench    - run the benchmark (BENCH_ARGS=<blocks>)
#   make fuzz     - run the libFuzzer target with clang++ (FUZZ_SECONDS=60)
#   make clean

CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     := -Wall -Wextra -Wpedantic -Werror
CXXFLAGS ?= -O2 -g
SANITIZE := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all

BUILD := build
LIB   := src/allocator.cpp
HDRS  := include/allocator.h
# the latest tag (`git describe`): 0.0.9, or 0.0.9-2-gabc1234 after it
ALLOCATOR_VERSION ?= $(shell git describe --tags --match 'v[0-9]*' 2>/dev/null | sed 's/^v//')
INC   := -Iinclude -DALLOCATOR_VERSION='"$(or $(ALLOCATOR_VERSION),unknown)"'
TESTS := test/allocator_test.cpp
# GoogleTest: libgtest-dev (Debian/Ubuntu), googletest (Homebrew)
GTEST ?= -lgtest_main -lgtest -pthread

FUZZ_CXX     ?= clang++
FUZZ_SECONDS ?= 60

.PHONY: all test sanitize demo bench fuzz clean
all: $(BUILD)/demo $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/demo: examples/demo.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) examples/demo.cpp $(LIB) -o $@

$(BUILD)/allocator_test: $(TESTS) $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) $(TESTS) $(LIB) $(GTEST) -o $@

$(BUILD)/allocator_test_san: $(TESTS) $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) -O1 -g $(SANITIZE) $(INC) $(TESTS) $(LIB) $(GTEST) -o $@

$(BUILD)/allocator_bench: bench/allocator_bench.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) -O2 -DNDEBUG $(INC) bench/allocator_bench.cpp $(LIB) -o $@

# the library is compiled into the target: libFuzzer needs its coverage
$(BUILD)/allocator_fuzz: fuzz/allocator_fuzz.cpp $(LIB) $(HDRS) | $(BUILD)
	$(FUZZ_CXX) $(CXXSTD) $(WARN) -O1 -g -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all $(INC) fuzz/allocator_fuzz.cpp $(LIB) -o $@

fuzz: $(BUILD)/allocator_fuzz
	./$(BUILD)/allocator_fuzz -max_total_time=$(FUZZ_SECONDS) -print_final_stats=1

bench: $(BUILD)/allocator_bench
	./$(BUILD)/allocator_bench $(BENCH_ARGS)

demo: $(BUILD)/demo
	./$(BUILD)/demo

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

sanitize: $(BUILD)/allocator_test_san
	./$(BUILD)/allocator_test_san

clean:
	rm -rf $(BUILD)
