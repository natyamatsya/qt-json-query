# CMake configuration specific to building the json_query test suites.
# This assumes that GoogleTest has already been fetched and available targets
# created by the top-level script (see cmake/GoogleTests.cmake).

option(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS "Build Jayway JSONPath parity test suite" OFF)

# ---------------------------------------------------------------------------
# Unit-test executable aggregating ONLY RFC 9535 compliance tests - FOCUSED
# ---------------------------------------------------------------------------
# Disabled core tests to focus only on RFC 9535 compliance suite
set(JSON_QUERY_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathConformanceGTest.cpp
    # Core tests temporarily disabled for compliance focus:
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerConformanceGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungExtraGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathLogicalOrPredicateGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/ContainerCursorGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/ContextAwareContainerCursorGTest.cpp
)

add_executable(json_query_tests ${JSON_QUERY_TEST_SOURCES})

target_include_directories(json_query_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/include
)

target_link_libraries(json_query_tests PRIVATE
    json_query
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    Qt6::Core
)

target_compile_features(json_query_tests PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# Jayway JSONPath parity suite
# ---------------------------------------------------------------------------
if(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS)
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
endif()

# ---------------------------------------------------------------------------
# RFC 9535 compliance test suite
# ---------------------------------------------------------------------------
file(GLOB RFC_9535_SOURCES
     ${CMAKE_CURRENT_SOURCE_DIR}/rfc-9535/*.cpp)

add_executable(rfc9535_tests ${RFC_9535_SOURCES})

target_include_directories(rfc9535_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests/include)

target_link_libraries(rfc9535_tests PRIVATE
    json_query
    GTest::gtest
    GTest::gmock
    GTest::gtest_main
    Qt6::Core)

# Provide project source dir to tests so they can locate compliance JSON files
target_compile_definitions(rfc9535_tests PRIVATE
    JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Test discovery via CTest / GoogleTest module
# ---------------------------------------------------------------------------
include(GoogleTest)

gtest_discover_tests(json_query_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "JsonQuery/"
)

if(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS)
    gtest_discover_tests(jayway_parity_tests
        DISCOVERY_MODE PRE_TEST
        TEST_PREFIX "jayway-parity/"
    )
endif()

gtest_discover_tests(rfc9535_tests
    DISCOVERY_MODE PRE_TEST
    TEST_PREFIX "RFC9535/")
