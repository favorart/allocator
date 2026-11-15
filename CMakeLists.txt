cmake_minimum_required(VERSION 3.14)

# The version comes from the latest tag (`git describe`): 0.0.9, or 0.0.9-2-gabc1234
# after it. CI sets ALLOCATOR_VERSION to the tag a master build is released under.
set(ALLOCATOR_VERSION "$ENV{ALLOCATOR_VERSION}" CACHE STRING "Library version (default: git describe)")
if(NOT ALLOCATOR_VERSION)
  find_program(GIT_EXECUTABLE git)
  if(GIT_EXECUTABLE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE git_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(REGEX REPLACE "^v" "" ALLOCATOR_VERSION "${git_version}")
  endif()
  if(NOT ALLOCATOR_VERSION)
    set(ALLOCATOR_VERSION unknown)
  endif()
endif()
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" project_version "${ALLOCATOR_VERSION}")
if(NOT project_version)
  set(project_version 0.0.0)
endif()

project(allocator VERSION ${project_version} LANGUAGES CXX)
message(STATUS "allocator ${ALLOCATOR_VERSION}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ALLOCATOR_BUILD_TESTS "Build the tests (needs GoogleTest)" ON)
option(ALLOCATOR_BUILD_BENCH "Build the benchmark" OFF)
option(ALLOCATOR_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

# GoogleTest: the system one, downloaded only when there is none.
# Added before the warning flags, which are for the project's own code.
if(ALLOCATOR_BUILD_TESTS)
  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND)
    message(STATUS "GoogleTest: system ${GTest_VERSION}")
  else()
    message(STATUS "GoogleTest: not installed, downloading v1.15.2")
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
  endif()
endif()

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
target_compile_definitions(allocator PRIVATE ALLOCATOR_VERSION="${ALLOCATOR_VERSION}")

add_executable(demo examples/demo.cpp)
target_link_libraries(demo PRIVATE allocator)

if(ALLOCATOR_BUILD_TESTS)
  enable_testing()
  add_executable(allocator_test test/allocator_test.cpp)
  target_link_libraries(allocator_test PRIVATE allocator GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(allocator_test)
endif()

if(ALLOCATOR_BUILD_BENCH)
  add_executable(allocator_bench bench/allocator_bench.cpp)
  target_link_libraries(allocator_bench PRIVATE allocator)
endif()
