# CMake configuration specific to building the json_query test suites.
# This assumes that GoogleTest has already been fetched and available targets
# created by the top-level script (see cmake/GoogleTests.cmake).

# ---------------------------------------------------------------------------
# Unit-test executable aggregating the core library tests
# ---------------------------------------------------------------------------
set(JSON_QUERY_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathConformanceGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerConformanceGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungExtraGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathLogicalOrPredicateGTest.cpp
)

add_executable(json_query_tests ${JSON_QUERY_TEST_SOURCES})

target_include_directories(json_query_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/include
)

target_link_libraries(json_query_tests PRIVATE
    json_query
    GTest::gtest
    GTest::gtest_main
    Qt6::Core
)

# ---------------------------------------------------------------------------
# Jayway JSONPath parity suite
# ---------------------------------------------------------------------------
set(JAYWAY_PARITY_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/DeepScanParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/MultiPropParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/FilterParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/EscapeParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/OptionsParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/MapperParityGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/jayway-parity/PathCompilerParityGTest.cpp
)

add_executable(jayway_parity_tests ${JAYWAY_PARITY_SOURCES})

target_include_directories(jayway_parity_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/include
)

target_link_libraries(jayway_parity_tests PRIVATE
    json_query
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    Qt6::Core
)

# ---------------------------------------------------------------------------
# Test discovery via CTest / GoogleTest module
# ---------------------------------------------------------------------------
include(GoogleTest)

gtest_discover_tests(json_query_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "JsonQuery/"
)

gtest_discover_tests(jayway_parity_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "jayway-parity/"
)
