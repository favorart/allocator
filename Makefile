# Allocator + Pointer: build and test targets.
#   make       - build the tests
#   make test  - run the tests
#   make clean

CXX      ?= g++
CXXSTD   ?= -std=c++17
WARN     := -Wall -Wextra
CXXFLAGS ?= -O2 -g

BUILD := build
LIB   := allocator.cpp
HDRS  := allocator.h

.PHONY: all test clean
all: $(BUILD)/allocator_test

$(BUILD):
	mkdir -p $@

$(BUILD)/allocator_test: main.cpp allocator_test.cpp $(LIB) $(HDRS) | $(BUILD)
	$(CXX) $(CXXSTD) $(WARN) $(CXXFLAGS) main.cpp allocator_test.cpp $(LIB) -o $@

test: $(BUILD)/allocator_test
	./$(BUILD)/allocator_test

clean:
	rm -rf $(BUILD)
