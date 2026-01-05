# ExtensionTests.cmake
# ---------------------------------------------------------------------------
# Optional extension tests (disabled by default)
#
# These tests require additional dependencies or are not part of the core
# functionality. They are kept for compatibility but disabled by default.
# ---------------------------------------------------------------------------

option(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS
       "Build Jayway JSONPath parity test suite" OFF)

option(JSON_QUERY_ENABLE_BAELDUNG_TESTS
       "Build Baeldung JSONPath extension test suite" OFF)

# Extension test sources (when enabled)
set(EXTENSION_TEST_SOURCES)

# Add Jayway parity tests if enabled and sources exist
if(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS)
  file(GLOB JAYWAY_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/extensions/jayway/*.cpp)
  if(JAYWAY_SOURCES)
    list(APPEND EXTENSION_TEST_SOURCES ${JAYWAY_SOURCES})
  endif()
endif()

# Add Baeldung tests if enabled and sources exist
if(JSON_QUERY_ENABLE_BAELDUNG_TESTS)
  file(GLOB BAELDUNG_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/extensions/jayway/*.cpp)
  if(BAELDUNG_SOURCES)
    list(APPEND EXTENSION_TEST_SOURCES ${BAELDUNG_SOURCES})
    message(STATUS "Found Baeldung extension tests: ${BAELDUNG_SOURCES}")
  endif()
endif()

# Add other extension tests here as needed file(GLOB PERFORMANCE_SOURCES
# ${CMAKE_CURRENT_SOURCE_DIR}/extensions/performance/*.cpp)
# if(PERFORMANCE_SOURCES) list(APPEND EXTENSION_TEST_SOURCES
# ${PERFORMANCE_SOURCES}) endif()

function(add_extension_tests)
  if(EXTENSION_TEST_SOURCES)
    add_executable(json_query_extension_tests ${EXTENSION_TEST_SOURCES})

    target_include_directories(
      json_query_extension_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                         ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      json_query_extension_tests PRIVATE json_query GTest::gmock
                                         GTest::gtest_main Qt6::Core)

    target_compile_features(json_query_extension_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(json_query_extension_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "Extensions/")

    message(STATUS "Added extension tests target: json_query_extension_tests")
  else()
    if(JSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS
       OR JSON_QUERY_ENABLE_BAELDUNG_TESTS)
      message(STATUS "Extension tests enabled but no sources found")
    else()
      message(
        STATUS
          "Extension tests disabled (use -DJSON_QUERY_ENABLE_JAYWAY_PARITY_TESTS=ON or -DJSON_QUERY_ENABLE_BAELDUNG_TESTS=ON to enable)"
      )
    endif()
  endif()
endfunction()
