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
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaKeywordTests.cpp)

    add_executable(json_schema_tests ${JSON_SCHEMA_TEST_SOURCES})

    target_link_libraries(json_schema_tests PRIVATE json_query GTest::gtest_main)

    target_include_directories(
        json_schema_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                  ${PROJECT_SOURCE_DIR}/include)

    gtest_discover_tests(json_schema_tests PREFIX "JSONSchema/")

    message(STATUS "Added JSON Schema tests target: json_schema_tests")
endfunction()
