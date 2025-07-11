# Enable testing
enable_testing()

# GoogleTest setup
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# List of GoogleTest sources
set(TEST_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/JSONPathGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPathConformanceGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPointerConformanceGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPointerGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPathBaeldungGTest.cpp
)

# Unified test executable
add_executable(json_query_tests ${TEST_SOURCES})

# Include project headers & link libs
target_include_directories(json_query_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(json_query_tests PRIVATE json_query GTest::gtest GTest::gtest_main Qt6::Core)

# Register with CTest
add_test(NAME JsonQueryTests COMMAND json_query_tests)
