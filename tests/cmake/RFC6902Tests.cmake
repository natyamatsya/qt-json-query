# RFC6902Tests.cmake
# ---------------------------------------------------------------------------
# RFC 6902 JSON Patch compliance tests (community json-patch-tests suite,
# vendored as the compliance/json-patch-tests git submodule) plus the
# JSONPatch unit tests.
# ---------------------------------------------------------------------------

set(RFC6902_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/rfc-6902/RFC6902ComplianceGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-patch/JSONPatchGTest.cpp
)

function(add_rfc6902_tests)
  # The compliance data comes from a git submodule; give an actionable hint
  # when it has not been checked out (the unit tests still build and run).
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/compliance/json-patch-tests/tests.json")
    message(
      WARNING
        "compliance/json-patch-tests submodule not found — RFC 6902 compliance "
        "cases will be empty. Populate it with:\n"
        "  git submodule update --init compliance/json-patch-tests")
  endif()

  add_executable(rfc6902_compliance_tests ${RFC6902_TEST_SOURCES})

  target_include_directories(
    rfc6902_compliance_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                     ${PROJECT_SOURCE_DIR}/tests/include)

  target_link_libraries(
    rfc6902_compliance_tests PRIVATE json_query::json_query GTest::gmock
                                     GTest::gtest_main Qt6::Core)

  # Provide project source dir to tests so they can locate compliance JSON
  # files
  target_compile_definitions(rfc6902_compliance_tests
                             PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

  target_compile_features(rfc6902_compliance_tests PRIVATE cxx_std_23)

  # Test discovery
  gtest_discover_tests(rfc6902_compliance_tests DISCOVERY_MODE PRE_TEST
                       TEST_PREFIX "RFC6902/")

  message(
    STATUS "Added RFC 6902 compliance tests target: rfc6902_compliance_tests")
endfunction()
