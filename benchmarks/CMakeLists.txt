# Benchmarks/CMakeLists.txt - build json_query benchmark executable

# Expect that Google Benchmark has been made available by
#    include(cmake/GoogleBenchmarkSetup.cmake)
# in the parent scope.

add_executable(json_benchmark Benchmark.cpp)

# Disable AUTOMOC for speed; benchmarks do not use Qt meta-object system.
set_target_properties(json_benchmark PROPERTIES AUTOMOC OFF)

# Include project headers
target_include_directories(json_benchmark PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link required libraries
find_package(Qt6 REQUIRED COMPONENTS Core)

target_link_libraries(json_benchmark PRIVATE
    json_query
    Qt6::Core
    benchmark::benchmark
)

# Optional: Register with CTest so benchmarks can be run via `ctest -R Bench`
if (BUILD_TESTING)
    add_test(NAME JSONBenchmark COMMAND json_benchmark --benchmark_color=auto)
    set_tests_properties(JSONBenchmark PROPERTIES TIMEOUT 120)
endif()
