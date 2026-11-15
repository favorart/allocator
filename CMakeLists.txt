cmake_minimum_required(VERSION 3.14)
project(allocator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALLOCATOR_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

# GoogleTest; added before the warning flags, which are for the project's own code
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/releases/download/v1.15.2/googletest-1.15.2.tar.gz
  URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926)
FetchContent_MakeAvailable(googletest)

if(MSVC)
  add_compile_options(/W4 /WX /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
  if(ALLOCATOR_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
  endif()
endif()

add_library(allocator STATIC src/allocator.cpp)
target_include_directories(allocator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(demo examples/demo.cpp)
target_link_libraries(demo PRIVATE allocator)

enable_testing()
add_executable(allocator_test test/allocator_test.cpp)
target_link_libraries(allocator_test PRIVATE allocator GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(allocator_test)
