# RFC9535Tests.cmake
# ---------------------------------------------------------------------------
# RFC 9535 JSONPath specification compliance tests
# ---------------------------------------------------------------------------

# RFC 9535 compliance test sources
set(RFC9535_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/rfc-9535/RFC9535CTSComplianceGTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/rfc-9535/JSONPathErrorGTest.cpp
)

function(add_rfc9535_tests)
  if(RFC9535_TEST_SOURCES)
    add_executable(rfc9535_compliance_tests ${RFC9535_TEST_SOURCES})

    target_include_directories(
      rfc9535_compliance_tests PRIVATE ${PROJECT_SOURCE_DIR}/include
                                       ${PROJECT_SOURCE_DIR}/tests/include)

    target_link_libraries(
      rfc9535_compliance_tests
      PRIVATE json_query function_ref_interface GTest::gtest GTest::gmock
              GTest::gtest_main Qt6::Core)

    # Provide project source dir to tests so they can locate compliance JSON
    # files
    target_compile_definitions(
      rfc9535_compliance_tests
      PRIVATE JSON_QUERY_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    target_compile_features(rfc9535_compliance_tests PRIVATE cxx_std_23)

    # Test discovery
    gtest_discover_tests(rfc9535_compliance_tests DISCOVERY_MODE PRE_TEST
                         TEST_PREFIX "RFC9535/")

    message(
      STATUS "Added RFC 9535 compliance tests target: rfc9535_compliance_tests")
  else()
    message(STATUS "No RFC 9535 test sources found")
  endif()
endfunction()
