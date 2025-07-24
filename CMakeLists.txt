cmake_minimum_required(VERSION 3.24)

project(json_query VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Testing option (enabled by default) and CTest include
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build and enable tests" ON)
include(CTest)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow the user to specify a Qt installation via CMAKE_PREFIX_PATH
# Example:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
include(FetchContent)

# ---------------------------------------------------------------------------
# Header-only CTRE (compile-time regular expressions)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    ctre
    GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
    GIT_TAG main # any reasonably recent tag
    SOURCE_SUBDIR single-header
)
FetchContent_MakeAvailable(ctre)

# Create an interface target for the single-header library if none provided
if(NOT TARGET ctre_interface)
    add_library(ctre_interface INTERFACE)
    target_include_directories(ctre_interface INTERFACE ${ctre_SOURCE_DIR}/single-header)
endif()

add_subdirectory(src/json-query)

# Add debug executable for test analysis
add_executable(debug_test debug_test.cpp)
target_link_libraries(debug_test PRIVATE json_query Qt6::Core)
target_include_directories(debug_test PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json-query
)

# Add error audit script for RFC 9535 compliance analysis
add_executable(error_audit_script error_audit_script.cpp)
target_link_libraries(error_audit_script PRIVATE json_query Qt6::Core)
target_include_directories(error_audit_script PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json-query
)

# Add CTS cross-reference script for detailed CTS compliance analysis
add_executable(cts_error_cross_reference cts_error_cross_reference.cpp)
target_link_libraries(cts_error_cross_reference PRIVATE json_query Qt6::Core)
target_include_directories(cts_error_cross_reference PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json-query
)

# Add RFC 9535 terminology audit script for terminology compliance analysis
add_executable(rfc9535_terminology_audit rfc9535_terminology_audit.cpp)
target_link_libraries(rfc9535_terminology_audit PRIVATE json_query Qt6::Core)
target_include_directories(rfc9535_terminology_audit PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/json-query
)

# Debug test executable for investigating root array evaluation issue
add_executable(debug_root_array_test debug_root_array_test.cpp)
target_link_libraries(debug_root_array_test PRIVATE json_query Qt6::Core)
target_compile_features(debug_root_array_test PRIVATE cxx_std_23)

# Debug test executable for CTS root test exact replication
add_executable(debug_cts_root_test debug_cts_root_test.cpp)
target_link_libraries(debug_cts_root_test PRIVATE json_query Qt6::Core)
target_compile_features(debug_cts_root_test PRIVATE cxx_std_23)

# Debug test executable for investigating specific issues
add_executable(debug_root_test debug_root_test.cpp)
target_link_libraries(debug_root_test PRIVATE json_query Qt6::Core)
target_compile_features(debug_root_test PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# Examples and tests
# ---------------------------------------------------------------------------
# Build example executables (simple_pointer, simple_path, ...)
add_subdirectory(examples)

# Unit tests (GoogleTest)
if (BUILD_TESTING)
    include(cmake/GoogleTestSetup.cmake)
    add_subdirectory(tests)

    # ---------------------------------------------------------------------------
    # ContainerCursor STL Interface Demo
    # ---------------------------------------------------------------------------
    add_executable(test_container_cursor_stl test_container_cursor_stl.cpp)
    target_link_libraries(test_container_cursor_stl PRIVATE json_query Qt6::Core)
    target_compile_features(test_container_cursor_stl PRIVATE cxx_std_23)

    # ---------------------------------------------------------------------------
    # Context-Aware ContainerCursor Test
    # ---------------------------------------------------------------------------
    add_executable(test_context_aware_cursor test_context_aware_cursor.cpp)
    target_link_libraries(test_context_aware_cursor PRIVATE json_query Qt6::Core)
    target_compile_features(test_context_aware_cursor PRIVATE cxx_std_23)

endif()

# Benchmarks
if (BUILD_BENCHMARKING)
    include(cmake/GoogleBenchmarkSetup.cmake)
    add_subdirectory(benchmarks)
endif()

# Profiling test for performance analysis
add_executable(profile_test profile_test.cpp)
target_link_libraries(profile_test PRIVATE json_query Qt6::Core)
target_compile_features(profile_test PRIVATE cxx_std_23)

# Include gperftools for profiling if available
find_path(GPERFTOOLS_INCLUDE_DIR gperftools/profiler.h HINTS /opt/homebrew/include)
find_library(GPERFTOOLS_PROFILER_LIB profiler HINTS /opt/homebrew/lib)

if(GPERFTOOLS_INCLUDE_DIR AND GPERFTOOLS_PROFILER_LIB)
    target_include_directories(profile_test PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
    target_link_libraries(profile_test PRIVATE ${GPERFTOOLS_PROFILER_LIB})
    target_compile_definitions(profile_test PRIVATE HAVE_GPERFTOOLS)
endif()

# ---------------------------------------------------------------------------
# Installation (headers and library)
# ---------------------------------------------------------------------------
if(JSON_QUERY_ENABLE_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS json_query ctre_interface EXPORT json_queryTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # Export for downstream find_package(json_query)
    install(EXPORT json_queryTargets
            FILE json_queryTargets.cmake
            NAMESPACE json_query::
            DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)
endif()
