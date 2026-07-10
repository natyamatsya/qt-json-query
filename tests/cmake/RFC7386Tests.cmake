# RFC7386Tests.cmake
# ---------------------------------------------------------------------------
# RFC 7386 JSON Merge Patch compliance tests (data-driven from the complete
# Appendix A example table plus edge cases; rfc7386-tests.json).
# ---------------------------------------------------------------------------

set(RFC7386_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/rfc-7386/RFC7386ComplianceGTest.cpp
)

function(add_rfc7386_tests)
  add_executable(rfc7386_compliance_tests ${RFC7386_TEST_SOURCES})

  target_include_directories(
    rfc7386_compliance_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                     ${PROJECT_SOURCE_DIR}/tests/include)

  target_link_libraries(
    rfc7386_compliance_tests PRIVATE json_query::json_query GTest::gmock
                                     GTest::gtest_main Qt6::Core)

  # Provide project source dir to tests so they can locate the test JSON file
  target_compile_definitions(rfc7386_compliance_tests
                             PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

  target_compile_features(rfc7386_compliance_tests PRIVATE cxx_std_23)

  # Test discovery
  gtest_discover_tests(rfc7386_compliance_tests DISCOVERY_MODE PRE_TEST
                       TEST_PREFIX "RFC7386/")

  message(
    STATUS "Added RFC 7386 compliance tests target: rfc7386_compliance_tests")
endfunction()
