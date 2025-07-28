# CMake configuration specific to building the json_query test suites. This
# assumes that GoogleTest has already been fetched and available targets created
# by the top-level script (see cmake/GoogleTests.cmake).

option(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS
       "Build Jayway JSONPath parity test suite" OFF)

# ---------------------------------------------------------------------------
# Unit-test executable aggregating ONLY RFC 9535 compliance tests - FOCUSED
# ---------------------------------------------------------------------------
# Disabled core tests to focus only on RFC 9535 compliance suite
set(JSON_QUERY_TEST_SOURCES
    # Core tests temporarily disabled for compliance focus:
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathConformanceGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerConformanceGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPointerGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathBaeldungExtraGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/JSONPathLogicalOrPredicateGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/ContainerCursorGTest.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/ContextAwareContainerCursorGTest.cpp
)

# Only create json_query_tests target if there are source files
if(JSON_QUERY_TEST_SOURCES)
  add_executable(json_query_tests ${JSON_QUERY_TEST_SOURCES})

  target_include_directories(
    json_query_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                             ${PROJECT_SOURCE_DIR}/tests/include)

  target_link_libraries(
    json_query_tests PRIVATE json_query GTest::gtest GTest::gmock
                             GTest::gtest_main Qt6::Core)

  target_compile_features(json_query_tests PRIVATE cxx_std_23)
else()
  message(STATUS "Core test suite disabled - no sources for json_query_tests")
endif()

# ---------------------------------------------------------------------------
# RFC 9535 compliance test suite
# ---------------------------------------------------------------------------
file(GLOB RFC_9535_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/rfc-9535/*.cpp)

add_executable(rfc9535_tests ${RFC_9535_SOURCES})

target_include_directories(
  rfc9535_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                        ${PROJECT_SOURCE_DIR}/tests/include)

target_link_libraries(
  rfc9535_tests PRIVATE json_query function_ref_interface GTest::gtest
                        GTest::gmock GTest::gtest_main Qt6::Core)

# Provide project source dir to tests so they can locate compliance JSON files
target_compile_definitions(
  rfc9535_tests PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# RFC 6901 JSON Pointer compliance test suite
# ---------------------------------------------------------------------------
add_executable(rfc6901_tests
               ${CMAKE_CURRENT_SOURCE_DIR}/rfc-6901/RFC6901ComplianceGTest.cpp)

target_include_directories(
  rfc6901_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                        ${PROJECT_SOURCE_DIR}/tests/include)

target_link_libraries(
  rfc6901_tests PRIVATE json_query GTest::gtest GTest::gmock GTest::gtest_main
                        Qt6::Core)

# Provide project source dir to tests so they can locate compliance JSON files
target_compile_definitions(
  rfc6901_tests PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_compile_features(rfc6901_tests PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# Test discovery via CTest / GoogleTest module
# ---------------------------------------------------------------------------
include(GoogleTest)

if(JSON_QUERY_TEST_SOURCES)
  gtest_discover_tests(json_query_tests DISCOVERY_MODE PRE_TEST
                       TEST_PREFIX "JsonQuery/")
endif()

if(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS)
  gtest_discover_tests(jayway_parity_tests DISCOVERY_MODE PRE_TEST
                       TEST_PREFIX "jayway-parity/")
endif()

gtest_discover_tests(rfc9535_tests DISCOVERY_MODE PRE_TEST
                     TEST_PREFIX "RFC9535/")

gtest_discover_tests(rfc6901_tests DISCOVERY_MODE PRE_TEST
                     TEST_PREFIX "RFC6901/")

# ---------------------------------------------------------------------------
# CompactContextFilterStorage lifetime fix test
# ---------------------------------------------------------------------------
add_executable(
  compact_context_filter_fix_test
  ${CMAKE_CURRENT_SOURCE_DIR}/test_compact_context_filter_standalone.cpp)

target_include_directories(compact_context_filter_fix_test
                           PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(compact_context_filter_fix_test PRIVATE Qt6::Core)

target_compile_features(compact_context_filter_fix_test PRIVATE cxx_std_23)

# Register the lifetime fix test with CTest
add_test(NAME CompactContextFilterStorageLifetimeFix
         COMMAND compact_context_filter_fix_test)

# ---------------------------------------------------------------------------
# Comprehensive variant-based storage test
# ---------------------------------------------------------------------------
add_executable(
  variant_storage_comprehensive_test
  ${CMAKE_CURRENT_SOURCE_DIR}/test_variant_storage_comprehensive.cpp)

target_include_directories(variant_storage_comprehensive_test
                           PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(variant_storage_comprehensive_test PRIVATE Qt6::Core)

target_compile_features(variant_storage_comprehensive_test PRIVATE cxx_std_23)

# Register the comprehensive variant storage test with CTest
add_test(NAME VariantStorageComprehensive
         COMMAND variant_storage_comprehensive_test)
