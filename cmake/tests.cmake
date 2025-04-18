# Enable testing
enable_testing()

# Tests requiring QtTest
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

# Add test executables
add_executable(jsonpointer_test tests/jsonpointertest.cpp)
target_link_libraries(jsonpointer_test PRIVATE 
    json_query 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Test
)

add_executable(jsonpath_test tests/jsonpathtest.cpp)
target_link_libraries(jsonpath_test PRIVATE 
    json_query 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Test
)

add_executable(benchmark_test tests/benchmark.cpp)
target_link_libraries(benchmark_test PRIVATE 
    json_query 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Test
)

# Register tests with CTest
add_test(NAME JSONPointerTest COMMAND jsonpointer_test)
add_test(NAME JSONPathTest COMMAND jsonpath_test)
add_test(NAME BenchmarkTest COMMAND benchmark_test)

# Set test properties
set_tests_properties(JSONPointerTest PROPERTIES TIMEOUT 60)
set_tests_properties(JSONPathTest PROPERTIES TIMEOUT 60)
set_tests_properties(BenchmarkTest PROPERTIES 
    TIMEOUT 120
    LABELS "benchmark"
)
