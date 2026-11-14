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
LIB   := allocator.cpp
HDRS  := allocator.h

.PHONY: all test sanitize demo clean
all: $(BUILD)/demo $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/demo: main.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) main.cpp $(LIB) -o $@

$(BUILD)/allocator_test: allocator_test.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) allocator_test.cpp $(LIB) -o $@

$(BUILD)/allocator_test_san: allocator_test.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) -O1 -g $(SANITIZE) allocator_test.cpp $(LIB) -o $@

demo: $(BUILD)/demo
	./$(BUILD)/demo

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

sanitize: $(BUILD)/allocator_test_san
	./$(BUILD)/allocator_test_san

clean:
	rm -rf $(BUILD)
