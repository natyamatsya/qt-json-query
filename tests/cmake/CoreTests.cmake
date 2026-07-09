# CoreTests.cmake
# ---------------------------------------------------------------------------
# Core JSONPath and JSONPointer functionality tests
# ---------------------------------------------------------------------------

# Core JSONPath tests
set(CORE_JSONPATH_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/JSONPathGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-path/JSONPathLogicalOrGTest.cpp)

# Core JSONPointer tests
set(CORE_JSONPOINTER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-pointer/JSONPointerGTest.cpp)

# Combined core test sources
set(CORE_TEST_SOURCES ${CORE_JSONPATH_SOURCES} ${CORE_JSONPOINTER_SOURCES})

function(add_core_tests)
  if(CORE_TEST_SOURCES)
    add_executable(json_query_core_tests ${CORE_TEST_SOURCES})

    target_include_directories(
      json_query_core_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                    ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      json_query_core_tests PRIVATE json_query::json_query GTest::gmock
                                    GTest::gtest_main Qt6::Core)

    target_compile_features(json_query_core_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(json_query_core_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "Core/")

    message(STATUS "Added core tests target: json_query_core_tests")
  else()
    message(STATUS "No core test sources found")
  endif()
endfunction()
