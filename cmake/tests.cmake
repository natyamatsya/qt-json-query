# Enable testing
enable_testing()

# Always use GoogleTest for unit tests
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/JSONPathGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPathConformanceGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPointerConformanceGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPointerGTest.cpp
)

add_executable(json_query_tests ${TEST_SOURCES})

# Project headers
target_include_directories(json_query_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(json_query_tests PRIVATE json_query GTest::gtest GTest::gtest_main Qt6::Core)

add_test(NAME JsonQueryTests COMMAND json_query_tests)

# Google Benchmark integration remains
include(${PROJECT_SOURCE_DIR}/cmake/Benchmarks.cmake)

    # GTest-based JSONPath tests
    add_executable(jsonpath_gtest ${PROJECT_SOURCE_DIR}/tests/JSONPathGTest.cpp)
    target_include_directories(jsonpath_gtest PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_link_libraries(jsonpath_gtest PRIVATE json_query GTest::gtest GTest::gtest_main Qt6::Core)
    add_test(NAME JSONPathTest COMMAND jsonpath_gtest)

    return()
endif()

# Add test executables
# JSON Pointer unit tests
add_executable(jsonpointer_test ${PROJECT_SOURCE_DIR}/tests/JSONPointerTest.cpp)

# JSON Pointer RFC-6901 conformance suite
add_executable(jsonpointer_conformance_test ${PROJECT_SOURCE_DIR}/tests/JSONPointerConformanceTest.cpp)
# Enable Qt MOC for both
set_target_properties(jsonpointer_test jsonpointer_conformance_test PROPERTIES AUTOMOC ON)
set_target_properties(jsonpointer_test PROPERTIES AUTOMOC ON)
# Include project headers
target_include_directories(jsonpointer_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(jsonpointer_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test)

target_link_libraries(jsonpointer_conformance_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test)

# JSONPath legacy/functional tests
add_executable(jsonpath_test ${PROJECT_SOURCE_DIR}/tests/JSONPathTest.cpp)

# JSONPath RFC/Draft conformance suite
add_executable(jsonpath_conformance_test ${PROJECT_SOURCE_DIR}/tests/JSONPathConformanceTest.cpp)
set_target_properties(jsonpath_test jsonpath_conformance_test PROPERTIES AUTOMOC ON)
target_include_directories(jsonpath_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(jsonpath_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test)

target_link_libraries(jsonpath_conformance_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test)


# ---------------------------------------------------------------------------
# Google Benchmark integration
# ---------------------------------------------------------------------------
include(${PROJECT_SOURCE_DIR}/cmake/Benchmarks.cmake)

# Register tests with CTest
add_test(NAME JSONPointerTest COMMAND jsonpointer_test)
add_test(NAME JSONPointerConformanceTest COMMAND jsonpointer_conformance_test)
add_test(NAME JSONPathTest COMMAND jsonpath_test)
add_test(NAME JSONPathConformanceTest COMMAND jsonpath_conformance_test)
# Benchmark test registration handled in Benchmarks.cmake

# Set test properties
set_tests_properties(JSONPointerTest PROPERTIES TIMEOUT 60)
set_tests_properties(JSONPointerConformanceTest PROPERTIES TIMEOUT 60)
set_tests_properties(JSONPathTest PROPERTIES TIMEOUT 60)
set_tests_properties(JSONPathConformanceTest PROPERTIES TIMEOUT 60)
