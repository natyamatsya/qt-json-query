# IETFJSONSchemaDraft2020_12Tests.cmake
# ---------------------------------------------------------------------------
# IETF JSON Schema Draft 2020-12 specification compliance tests
# ---------------------------------------------------------------------------

# IETF JSON Schema Draft 2020-12 compliance test sources
set(IETF_JSON_SCHEMA_DRAFT_2020_12_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/ietf-json-schema-draft-2020-12/IETFJSONSchemaComplianceGTest.cpp
)

set(IETF_JSON_SCHEMA_TEST_SUITE_DIR
    "${PROJECT_SOURCE_DIR}/compliance/ietf-json-schema-draft-2020-12")

function(add_ietf_json_schema_draft_2020_12_tests)
  # Check if test suite submodule is available
  if(EXISTS "${IETF_JSON_SCHEMA_TEST_SUITE_DIR}")
    add_executable(ietf_json_schema_draft_2020_12_compliance_tests
                   ${IETF_JSON_SCHEMA_DRAFT_2020_12_TEST_SOURCES})

    target_include_directories(
      ietf_json_schema_draft_2020_12_compliance_tests
      PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      ietf_json_schema_draft_2020_12_compliance_tests
      PRIVATE json_query GTest::gtest GTest::gmock GTest::gtest_main Qt6::Core)

    # Provide project source dir to tests so they can locate compliance JSON
    # files
    target_compile_definitions(
      ietf_json_schema_draft_2020_12_compliance_tests
      PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    target_compile_features(ietf_json_schema_draft_2020_12_compliance_tests
                            PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(
      ietf_json_schema_draft_2020_12_compliance_tests DISCOVERY_MODE PRE_TEST
      TEST_PREFIX "IETFJSONSchema/Draft2020-12/")

    message(
      STATUS
        "Added IETF JSON Schema Draft 2020-12 compliance tests target: ietf_json_schema_draft_2020_12_compliance_tests"
    )
  else()
    message(
      STATUS
        "IETF JSON Schema Test Suite not found at ${IETF_JSON_SCHEMA_TEST_SUITE_DIR}"
    )
    message(STATUS "To enable compliance tests, run:")
    message(
      STATUS
        "  git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git compliance/ietf-json-schema-draft-2020-12"
    )
    message(STATUS "  git submodule update --init --recursive")
  endif()
endfunction()
