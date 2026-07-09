# InternalTests.cmake
# ---------------------------------------------------------------------------
# Internal component tests for JSONPath implementation
# ---------------------------------------------------------------------------

# Internal component test sources
set(INTERNAL_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/internal/CompactContextFilterStorageGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/internal/ContainerCursorGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/internal/ContextAwareContainerCursorGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/internal/EmbeddedFilterStorageGTest.cpp
)

function(add_internal_tests)
  if(INTERNAL_TEST_SOURCES)
    add_executable(json_query_internal_tests ${INTERNAL_TEST_SOURCES})

    target_include_directories(
      json_query_internal_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                        ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      json_query_internal_tests PRIVATE json_query::json_query GTest::gmock
                                        GTest::gtest_main Qt6::Core)

    target_compile_features(json_query_internal_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(json_query_internal_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "Internal/")

    message(STATUS "Added internal tests target: json_query_internal_tests")
  else()
    message(STATUS "No internal test sources found")
  endif()
endfunction()
