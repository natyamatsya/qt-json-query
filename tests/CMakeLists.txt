# CMakeLists.txt
# ---------------------------------------------------------------------------
# Modular test suite configuration for json_query
#
# This file orchestrates the different test suites by including modular CMake
# files for each test category. Each module is self-contained and can be
# enabled/disabled independently.
# ---------------------------------------------------------------------------

# Ensure GoogleTest is available (assumed to be set up by parent CMake)
include(GoogleTest)

# Create cmake subdirectory for our modular test configurations
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# ---------------------------------------------------------------------------
# Test Suite Options
# ---------------------------------------------------------------------------

option(JSON_QUERY_ENABLE_CORE_TESTS
       "Build core JSONPath and JSONPointer functionality tests" ON)

option(JSON_QUERY_ENABLE_INTERNAL_TESTS "Build internal component tests" ON)

option(JSON_QUERY_ENABLE_RFC9535_TESTS
       "Build RFC 9535 JSONPath compliance tests" ON)

option(JSON_QUERY_ENABLE_RFC6901_TESTS
       "Build RFC 6901 JSON Pointer compliance tests" ON)

option(JSON_QUERY_ENABLE_RFC6902_TESTS
       "Build RFC 6902 JSON Patch compliance and unit tests" ON)

option(JSON_QUERY_ENABLE_SCHEMA_TESTS "Build JSON Schema validation tests" ON)

option(JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS
       "Build IETF JSON Schema Draft 2020-12 compliance tests" ON)

# ---------------------------------------------------------------------------
# Include Modular Test Suite Configurations
# ---------------------------------------------------------------------------

# Core functionality tests
if(JSON_QUERY_ENABLE_CORE_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CoreTests.cmake)
  add_core_tests()
endif()

# Internal component tests
if(JSON_QUERY_ENABLE_INTERNAL_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/InternalTests.cmake)
  add_internal_tests()
endif()

# RFC 9535 JSONPath compliance tests
if(JSON_QUERY_ENABLE_RFC9535_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RFC9535Tests.cmake)
  add_rfc9535_tests()
endif()

# RFC 6901 JSON Pointer compliance tests
if(JSON_QUERY_ENABLE_RFC6901_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RFC6901Tests.cmake)
  add_rfc6901_tests()
endif()

# RFC 6902 JSON Patch compliance + unit tests
if(JSON_QUERY_ENABLE_RFC6902_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/RFC6902Tests.cmake)
  add_rfc6902_tests()
endif()

# Fuzz tests (optional)
if(JSON_QUERY_ENABLE_FUZZ_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FuzzTests.cmake)
  add_fuzz_tests()
endif()

# JSON Schema validation tests
if(JSON_QUERY_ENABLE_SCHEMA_TESTS)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/JSONSchemaTests.cmake)
  add_json_schema_tests()
endif()

# IETF JSON Schema Draft 2020-12 compliance tests
if(JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS)
  include(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/IETFJSONSchemaDraft2020_12Tests.cmake)
  add_ietf_json_schema_draft_2020_12_tests()
endif()

# ---------------------------------------------------------------------------
# Aggregate Test Target (Optional)
# ---------------------------------------------------------------------------

option(JSON_QUERY_CREATE_AGGREGATE_TARGET
       "Create an aggregate target that runs all enabled test suites" OFF)

if(JSON_QUERY_CREATE_AGGREGATE_TARGET)
  # Create a custom target that depends on all enabled test executables
  set(ALL_TEST_TARGETS)

  if(JSON_QUERY_ENABLE_CORE_TESTS AND TARGET json_query_core_tests)
    list(APPEND ALL_TEST_TARGETS json_query_core_tests)
  endif()

  if(JSON_QUERY_ENABLE_INTERNAL_TESTS AND TARGET json_query_internal_tests)
    list(APPEND ALL_TEST_TARGETS json_query_internal_tests)
  endif()

  if(JSON_QUERY_ENABLE_RFC9535_TESTS AND TARGET rfc9535_compliance_tests)
    list(APPEND ALL_TEST_TARGETS rfc9535_compliance_tests)
  endif()

  if(JSON_QUERY_ENABLE_RFC6901_TESTS AND TARGET rfc6901_compliance_tests)
    list(APPEND ALL_TEST_TARGETS rfc6901_compliance_tests)
  endif()

  if(JSON_QUERY_ENABLE_RFC6902_TESTS AND TARGET rfc6902_compliance_tests)
    list(APPEND ALL_TEST_TARGETS rfc6902_compliance_tests)
  endif()

  if(JSON_QUERY_ENABLE_FUZZ_TESTS AND TARGET json_query_fuzz_tests)
    list(APPEND ALL_TEST_TARGETS json_query_fuzz_tests)
  endif()

  if(JSON_QUERY_ENABLE_SCHEMA_TESTS AND TARGET json_schema_tests)
    list(APPEND ALL_TEST_TARGETS json_schema_tests)
  endif()

  if(JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS
     AND TARGET ietf_json_schema_draft_2020_12_compliance_tests)
    list(APPEND ALL_TEST_TARGETS
         ietf_json_schema_draft_2020_12_compliance_tests)
  endif()

  if(ALL_TEST_TARGETS)
    add_custom_target(all_json_query_tests DEPENDS ${ALL_TEST_TARGETS})
    message(STATUS "Created aggregate target: all_json_query_tests")
  endif()
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

message(STATUS "=== JSON Query Test Suite Configuration ===")
message(STATUS "Core Tests:      ${JSON_QUERY_ENABLE_CORE_TESTS}")
message(STATUS "Internal Tests:  ${JSON_QUERY_ENABLE_INTERNAL_TESTS}")
message(STATUS "RFC 9535 Tests:  ${JSON_QUERY_ENABLE_RFC9535_TESTS}")
message(STATUS "RFC 6901 Tests:  ${JSON_QUERY_ENABLE_RFC6901_TESTS}")
message(STATUS "RFC 6902 Tests:  ${JSON_QUERY_ENABLE_RFC6902_TESTS}")
message(STATUS "Fuzz Tests:      ${JSON_QUERY_ENABLE_FUZZ_TESTS}")
message(STATUS "Schema Tests:    ${JSON_QUERY_ENABLE_SCHEMA_TESTS}")
message(
  STATUS
    "IETF JSON Schema Draft 2020-12: ${JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS}"
)
message(STATUS "Aggregate Target: ${JSON_QUERY_CREATE_AGGREGATE_TARGET}")
message(STATUS "=============================================")
