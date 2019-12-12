# Download and unpack googlebenchmark at configure time
configure_file(tests/BenchmarkCMakeLists.txt.in googlebenchmark-download/CMakeLists.txt)

set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark-download )
if(result)
  message(FATAL_ERROR "CMake step for googlebenchmark failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark-download )
if(result)
  message(FATAL_ERROR "Build step for googlebenchmark failed: ${result}")
endif()

# Add googlebenchmark directly to our build. This defines
# the benchmark and benchmark_main targets.
add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/benchmark
                 ${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark-build
                 EXCLUDE_FROM_ALL)

# The gtest/gtest_main targets carry header search path
# dependencies automatically when using CMake 2.8.11 or
# later. Otherwise we have to add them here ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${benchmark_SOURCE_DIR}/include")
endif()
