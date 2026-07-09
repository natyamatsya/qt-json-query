# RFC6901Tests.cmake
# ---------------------------------------------------------------------------
# RFC 6901 JSON Pointer specification compliance tests
# ---------------------------------------------------------------------------

# RFC 6901 compliance test sources
set(RFC6901_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/rfc-6901/RFC6901ComplianceGTest.cpp
)

function(add_rfc6901_tests)
  if(RFC6901_TEST_SOURCES)
    add_executable(rfc6901_compliance_tests ${RFC6901_TEST_SOURCES})

    target_include_directories(
      rfc6901_compliance_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                       ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      rfc6901_compliance_tests PRIVATE json_query::json_query GTest::gmock
                                       GTest::gtest_main Qt6::Core)

    # Provide project source dir to tests so they can locate compliance JSON
    # files
    target_compile_definitions(
      rfc6901_compliance_tests
      PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    target_compile_features(rfc6901_compliance_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(rfc6901_compliance_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "RFC6901/")

    message(
      STATUS "Added RFC 6901 compliance tests target: rfc6901_compliance_tests")
  else()
    message(STATUS "No RFC 6901 test sources found")
  endif()
endfunction()
