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
    ${PROJECT_SOURCE_DIR}/tests/JSONPathBaeldungExtraGTest.cpp
    ${PROJECT_SOURCE_DIR}/tests/JSONPathLogicalOrPredicateGTest.cpp
)

# Unified test executable
add_executable(json_query_tests ${TEST_SOURCES})

# Include project headers & link libs
target_include_directories(json_query_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(json_query_tests PRIVATE json_query GTest::gtest GTest::gtest_main Qt6::Core)

# Jayway parity tests ---------------------------------------------------------
set(JAYWAY_PARITY_SOURCES
    ${PROJECT_SOURCE_DIR}/tests/jayway-parity/JaywayParityGTest.cpp
)
add_executable(jayway_parity_tests ${JAYWAY_PARITY_SOURCES})
target_include_directories(jayway_parity_tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(jayway_parity_tests PRIVATE json_query GTest::gtest GTest::gtest_main Qt6::Core)

# Register with CTest, discover individual GoogleTest cases for both suites
include(GoogleTest)
gtest_discover_tests(json_query_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "JsonQuery/"
)

gtest_discover_tests(jayway_parity_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "jayway-parity/"
)
