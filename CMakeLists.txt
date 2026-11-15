cmake_minimum_required(VERSION 3.14)
project(allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra)
endif()

add_library(allocator STATIC src/allocator.cpp)
target_include_directories(allocator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

enable_testing()
add_executable(allocator_test test/main.cpp test/allocator_test.cpp)
target_link_libraries(allocator_test PRIVATE allocator)
add_test(NAME allocator_test COMMAND allocator_test)
