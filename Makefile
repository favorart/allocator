# Allocator + Pointer: build and test targets.
#   make       - build the tests
#   make test  - run the tests
#   make clean

CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     := -Wall -Wextra
CXXFLAGS ?= -O2 -g

BUILD := build
LIB   := src/allocator.cpp
HDRS  := include/allocator.h
INC   := -Iinclude
TESTS := test/main.cpp test/allocator_test.cpp

.PHONY: all test clean
all: $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/allocator_test: $(TESTS) $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) $(TESTS) $(LIB) -o $@

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

clean:
	rm -rf $(BUILD)
