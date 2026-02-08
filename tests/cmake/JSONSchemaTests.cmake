# JSONSchemaTests.cmake
# ---------------------------------------------------------------------------
# JSON Schema validation unit tests
# ---------------------------------------------------------------------------

# JSON Schema unit test sources
set(JSON_SCHEMA_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaBasicGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaFormatGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaKeywordGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaRefGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaRegistryGTest.cpp)

function(add_json_schema_tests)
  if(JSON_SCHEMA_TEST_SOURCES)
    add_executable(json_schema_tests ${JSON_SCHEMA_TEST_SOURCES})

    target_include_directories(
      json_schema_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(json_schema_tests PRIVATE json_query GTest::gmock
                                                    GTest::gtest_main Qt6::Core)

    target_compile_features(json_schema_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(json_schema_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "JSONSchema/")

    message(STATUS "Added JSON Schema tests target: json_schema_tests")
  else()
    message(STATUS "No JSON Schema test sources found")
  endif()
endfunction()
