# FuzzTests.cmake - LibFuzzer-based fuzz testing configuration
#
# This module defines fuzz testing targets using LibFuzzer for comprehensive
# robustness testing of the qt-json-query library components.

# Check if fuzzing is enabled and compiler supports it
if(NOT ENABLE_FUZZ_TESTS)
  return()
endif()

# Verify compiler support for LibFuzzer
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "Fuzz tests require Clang compiler with LibFuzzer support")
  return()
endif()

# Define fuzz test sources
set(FUZZ_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_simple_api.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_jsonpath_parsing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_jsonpointer_parsing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_combined_evaluation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_container_cursor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_filter_storage.cpp)

# Common fuzz test configuration
function(configure_fuzz_target target_name source_file)
  add_executable(${target_name} ${source_file})

  # Link against the main library and dependencies
  target_link_libraries(${target_name} PRIVATE json_query Qt6::Core)

  # Link CTRE interface if available
  if(TARGET ctre_interface)
    target_link_libraries(${target_name} PRIVATE ctre_interface)
  endif()

  # Add include directories for the library headers
  target_include_directories(${target_name} PRIVATE ${CMAKE_SOURCE_DIR}/include
                                                    ${CMAKE_SOURCE_DIR}/src)

  # Enable LibFuzzer and sanitizers
  target_compile_options(
    ${target_name}
    PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g
            -O1 # Light optimization for better fuzzing performance
  )

  target_link_options(${target_name} PRIVATE
                      -fsanitize=fuzzer,address,undefined)

  # Set C++23 standard
  target_compile_features(${target_name} PRIVATE cxx_std_23)

  # Add to fuzz tests group
  set_target_properties(
    ${target_name} PROPERTIES FOLDER "FuzzTests" RUNTIME_OUTPUT_DIRECTORY
                                                 "${CMAKE_BINARY_DIR}/fuzz")

  # Create corpus directory
  set(CORPUS_DIR "${CMAKE_BINARY_DIR}/fuzz/corpus/${target_name}")
  file(MAKE_DIRECTORY ${CORPUS_DIR})

  # Add custom target for running this fuzzer
  add_custom_target(
    run_${target_name}
    COMMAND ${target_name} ${CORPUS_DIR} -max_total_time=300
            -print_final_stats=1
    DEPENDS ${target_name}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    COMMENT "Running ${target_name} fuzzer for 5 minutes")
endfunction()

# Function to add fuzz tests
function(add_fuzz_tests)
  message(STATUS "Adding LibFuzzer-based fuzz tests")

  # Simple API fuzzer (basic public API testing)
  configure_fuzz_target(fuzz_simple_api
                        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_simple_api.cpp)

  # JSONPath parsing fuzzer
  configure_fuzz_target(
    fuzz_jsonpath_parsing
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_jsonpath_parsing.cpp)

  # JSON Pointer parsing fuzzer
  configure_fuzz_target(
    fuzz_jsonpointer_parsing
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_jsonpointer_parsing.cpp)

  # Combined evaluation fuzzer
  configure_fuzz_target(
    fuzz_combined_evaluation
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_combined_evaluation.cpp)

  # Container cursor fuzzer
  configure_fuzz_target(
    fuzz_container_cursor
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_container_cursor.cpp)

  # Filter storage fuzzer
  configure_fuzz_target(
    fuzz_filter_storage
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/fuzz_filter_storage.cpp)

  # Create aggregate target for running all fuzzers
  add_custom_target(
    run_all_fuzzers
    DEPENDS run_fuzz_simple_api run_fuzz_jsonpath_parsing
            run_fuzz_jsonpointer_parsing run_fuzz_combined_evaluation
            run_fuzz_container_cursor run_fuzz_filter_storage
    COMMENT "Running all fuzz tests")

  # Create target for quick fuzz testing (30 seconds each)
  add_custom_target(
    fuzz_quick
    COMMAND fuzz_simple_api ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_simple_api
            -max_total_time=30
    COMMAND
      fuzz_jsonpath_parsing
      ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_jsonpath_parsing -max_total_time=30
    COMMAND
      fuzz_jsonpointer_parsing
      ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_jsonpointer_parsing
      -max_total_time=30
    COMMAND
      fuzz_combined_evaluation
      ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_combined_evaluation
      -max_total_time=30
    COMMAND
      fuzz_container_cursor
      ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_container_cursor -max_total_time=30
    COMMAND
      fuzz_filter_storage ${CMAKE_BINARY_DIR}/fuzz/corpus/fuzz_filter_storage
      -max_total_time=30
    DEPENDS fuzz_simple_api fuzz_jsonpath_parsing fuzz_jsonpointer_parsing
            fuzz_combined_evaluation fuzz_container_cursor fuzz_filter_storage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/fuzz
    COMMENT "Running quick fuzz tests (30 seconds each)")

  message(
    STATUS
      "Fuzz tests configured. Use 'make run_all_fuzzers' or 'make fuzz_quick' to run."
  )
endfunction()

# Export the function for use in main CMakeLists.txt
set(FUZZ_TESTS_CONFIGURED
    TRUE
    CACHE INTERNAL "Fuzz tests have been configured")
