# GoogleBenchmarkSetup.cmake - fetch + configure Google Benchmark library

# Google Benchmark requires C++17+ and CMake 3.14+. We rely on FetchContent to
# download and build it at configure time so downstream users do not need a
# system installation.

include(FetchContent)

# Disable building Benchmark's own tests to speed up configure
set(BENCHMARK_ENABLE_TESTING
    OFF
    CACHE BOOL "Disable benchmark's own tests" FORCE)

FetchContent_Declare(
  googlebench
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3 # Adjust as needed
)

FetchContent_MakeAvailable(googlebench)
