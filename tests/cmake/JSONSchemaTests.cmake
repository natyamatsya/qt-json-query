# CMake module for JSON Schema validation tests
#
# This module provides the add_json_schema_tests() function that creates
# test targets for JSON Schema validation functionality.

function(add_json_schema_tests)
    # ---------------------------------------------------------------------------
    # JSON Schema Unit Tests
    # ---------------------------------------------------------------------------
    set(JSON_SCHEMA_TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaBasicTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaFormatTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaKeywordTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaRefTests.cpp)

    add_executable(json_schema_tests ${JSON_SCHEMA_TEST_SOURCES})

    target_link_libraries(json_schema_tests PRIVATE json_query GTest::gtest_main)

    target_include_directories(
        json_schema_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                  ${PROJECT_SOURCE_DIR}/include)

    gtest_discover_tests(json_schema_tests PREFIX "JSONSchema/")

    message(STATUS "Added JSON Schema tests target: json_schema_tests")

    # ---------------------------------------------------------------------------
    # JSON Schema Compliance Tests (Official Test Suite)
    # ---------------------------------------------------------------------------
    # Note: JSON Schema Draft 2020-12 is an IETF Internet-Draft, not an RFC
    # Check if test suite submodule is available (following RFC 9535 pattern)
    set(JSON_SCHEMA_TEST_SUITE_DIR "${PROJECT_SOURCE_DIR}/compliance/json-schema-test-suite")
    
    if(EXISTS "${JSON_SCHEMA_TEST_SUITE_DIR}")
        add_executable(json_schema_compliance_tests
            ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/ietf-json-schema-draft-2020-12/JSONSchemaTestSuiteRunner.cpp)

        target_link_libraries(json_schema_compliance_tests PRIVATE json_query GTest::gtest_main Qt6::Core)

        target_include_directories(
            json_schema_compliance_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                                  ${PROJECT_SOURCE_DIR}/include)

        # Provide project source dir to tests so they can locate compliance JSON files
        target_compile_definitions(json_schema_compliance_tests PRIVATE
            JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

        target_compile_features(json_schema_compliance_tests PRIVATE cxx_std_23)

        # Test discovery
        gtest_discover_tests(json_schema_compliance_tests DISCOVERY_MODE PRE_TEST
                             TEST_PREFIX "JSONSchema/Compliance/")

        message(STATUS "Added JSON Schema compliance tests target: json_schema_compliance_tests")
    else()
        message(STATUS "JSON Schema Test Suite not found at ${JSON_SCHEMA_TEST_SUITE_DIR}")
        message(STATUS "To enable compliance tests, run:")
        message(STATUS "  git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git compliance/json-schema-test-suite")
        message(STATUS "  git submodule update --init --recursive")
    endif()
endfunction()
