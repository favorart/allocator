# Allocator + Pointer: build, test and sanitizer targets.
#   make          - demo and tests
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

.PHONY: all test sanitize demo clean
all: $(BUILD)/demo $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/demo: examples/demo.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) examples/demo.cpp $(LIB) -o $@

$(BUILD)/allocator_test: test/allocator_test.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) $(INC) test/allocator_test.cpp $(LIB) -o $@

$(BUILD)/allocator_test_san: test/allocator_test.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) -O1 -g $(SANITIZE) $(INC) test/allocator_test.cpp $(LIB) -o $@

demo: $(BUILD)/demo
	./$(BUILD)/demo

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

sanitize: $(BUILD)/allocator_test_san
	./$(BUILD)/allocator_test_san

clean:
	rm -rf $(BUILD)
