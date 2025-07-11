# Benchmarks.cmake - build Google Benchmark-based benchmarks

include(FetchContent)

# ---------------------------------------------------------------------------
# Google Benchmark (header + static lib)
# ---------------------------------------------------------------------------
# We request a relatively recent commit/tag that has CMake support.
# Using FetchContent ensures that downstream users are not forced to have
# benchmark pre-installed.
FetchContent_Declare(
    googlebench
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3 # last tag known in training data – adjust if necessary
)
# Google Benchmark itself requires gtest for internal tests, but we disable them.
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark's own tests" FORCE)
FetchContent_MakeAvailable(googlebench)

# ---------------------------------------------------------------------------
# Benchmark executable
# ---------------------------------------------------------------------------
add_executable(json_benchmark ${PROJECT_SOURCE_DIR}/benchmarks/Benchmark.cpp)

# Make sure Qt's AUTOMOC is OFF for this target (not needed anymore)
set_target_properties(json_benchmark PROPERTIES AUTOMOC OFF)

# Includes
target_include_directories(json_benchmark PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Link libraries: json_query, QtCore, Google Benchmark
# Note: Google Benchmark target is "benchmark::benchmark" provided by its CMake.
find_package(Qt6 REQUIRED COMPONENTS Core)

target_link_libraries(json_benchmark PRIVATE
    json_query
    Qt6::Core
    benchmark::benchmark
)

# ---------------------------------------------------------------------------
# Register benchmark with CTest so it can be run via `ctest -R Bench`
# ---------------------------------------------------------------------------
add_test(NAME JSONBenchmark COMMAND json_benchmark --benchmark_color=auto)
set_tests_properties(JSONBenchmark PROPERTIES TIMEOUT 120)
