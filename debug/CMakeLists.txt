# CMakeLists.txt for debug tools Debug utilities for investigating specific
# issues and test failures

# Debug test executable for general test analysis
add_executable(debug_test src/debug_test.cpp)
target_link_libraries(debug_test PRIVATE json_query Qt6::Core)
target_include_directories(
  debug_test PRIVATE ${PROJECT_SOURCE_DIR}/include
                     ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(debug_test PRIVATE cxx_std_23)

# Debug test executable for investigating root array evaluation issue
add_executable(debug_root_array_test src/debug_root_array_test.cpp)
target_link_libraries(debug_root_array_test PRIVATE json_query Qt6::Core)
target_include_directories(
  debug_root_array_test PRIVATE ${PROJECT_SOURCE_DIR}/include
                                ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(debug_root_array_test PRIVATE cxx_std_23)

# Debug test executable for CTS root test exact replication
add_executable(debug_cts_root_test src/debug_cts_root_test.cpp)
target_link_libraries(debug_cts_root_test PRIVATE json_query Qt6::Core)
target_include_directories(
  debug_cts_root_test PRIVATE ${PROJECT_SOURCE_DIR}/include
                              ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(debug_cts_root_test PRIVATE cxx_std_23)

# Debug test executable for investigating specific issues
add_executable(debug_root_test src/debug_root_test.cpp)
target_link_libraries(debug_root_test PRIVATE json_query Qt6::Core)
target_include_directories(
  debug_root_test PRIVATE ${PROJECT_SOURCE_DIR}/include
                          ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(debug_root_test PRIVATE cxx_std_23)
