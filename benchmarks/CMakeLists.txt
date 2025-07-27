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

# Link function_ref header-only interface when available
if(TARGET function_ref_interface)
    target_link_libraries(json_benchmark PRIVATE function_ref_interface)
endif()
