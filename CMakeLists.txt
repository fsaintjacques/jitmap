cmake_minimum_required(VERSION 3.0.0)
project(jitmap)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/dev/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Builder unit tests" ON)
option(FORCE_COLORED_OUTPUT "Always produce ANSI-colored output (GNU/Clang only)." ON)

include(CompilerToolchain)
find_package(LLVM 8 REQUIRED CONFIG)

add_subdirectory(src/jitmap)
add_subdirectory(tools)

if (BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
  include(GoogleBenchmark)
  add_subdirectory(tests)
endif()
