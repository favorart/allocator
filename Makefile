# Allocator + Pointer: build, test and sanitizer targets.
#   make          - build the tests
#   make test     - run the tests
#   make sanitize - run the tests under ASan + UBSan
#   make clean

CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     := -Wall -Wextra -Wpedantic -Werror
CXXFLAGS ?= -O2 -g
SANITIZE := -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all

BUILD := build
LIB   := src/allocator.cpp
HDRS  := include/allocator.h
INC   := -Iinclude
TESTS := test/allocator_test.cpp

.PHONY: all test sanitize clean
all: $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/allocator_test: $(TESTS) $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) $(TESTS) $(LIB) -o $@

$(BUILD)/allocator_test_san: $(TESTS) $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) -O1 -g $(SANITIZE) $(INC) $(TESTS) $(LIB) -o $@

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

sanitize: $(BUILD)/allocator_test_san
	./$(BUILD)/allocator_test_san

clean:
	rm -rf $(BUILD)
