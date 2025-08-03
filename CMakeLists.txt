cmake_minimum_required(VERSION 3.24)

project(
  json_query
  VERSION 0.1
  LANGUAGES CXX)

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

# Refactoring tools option (disabled by default)
option(WITH_REFACTORING_TOOLS "Build C++ modernization refactoring tools" OFF)

# Allow the user to specify a Qt installation via CMAKE_PREFIX_PATH Example:
# cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos

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
  GIT_REPOSITORY
    https://github.com/hanickadot/compile-time-regular-expressions.git
  GIT_TAG main # any reasonably recent tag
  SOURCE_SUBDIR single-header)
FetchContent_MakeAvailable(ctre)

# Create an interface target for the single-header library if none provided
if(NOT TARGET ctre_interface)
  add_library(ctre_interface INTERFACE)
  target_include_directories(ctre_interface
                             INTERFACE ${ctre_SOURCE_DIR}/single-header)
endif()

# ---------------------------------------------------------------------------
# Header-only function_ref (lightweight non-owning function reference)
# ---------------------------------------------------------------------------
FetchContent_Declare(
  function_ref
  GIT_REPOSITORY https://github.com/TartanLlama/function_ref.git
  GIT_TAG master)

# Disable function_ref tests to avoid build issues
set(FUNCTION_REF_ENABLE_TESTS
    OFF
    CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(function_ref)

# Create an interface target for function_ref if none provided
if(NOT TARGET function_ref_interface)
  add_library(function_ref_interface INTERFACE)
  target_include_directories(function_ref_interface
                             INTERFACE ${function_ref_SOURCE_DIR}/include)
endif()

add_subdirectory(src/json-query)

# ---------------------------------------------------------------------------
# Examples and tests
# ---------------------------------------------------------------------------
# Build example executables (simple_pointer, simple_path, ...)
add_subdirectory(examples)

# Add organized subdirectories
add_subdirectory(compliance)
add_subdirectory(perf)

# Unit tests (GoogleTest)
if(BUILD_TESTING)
  include(cmake/GoogleTestSetup.cmake)
  add_subdirectory(tests)

endif()

# Benchmarks
if(BUILD_BENCHMARKING)
  include(cmake/GoogleBenchmarkSetup.cmake)
  add_subdirectory(benchmarks)
endif()

# ---------------------------------------------------------------------------
# Sanitizer Debug Test (for investigating sanitizer-induced test failures)
# ---------------------------------------------------------------------------
add_executable(sanitizer_debug_test sanitizer_debug_test.cpp)
target_link_libraries(sanitizer_debug_test PRIVATE Qt6::Core)
target_compile_features(sanitizer_debug_test PRIVATE cxx_std_23)

# ---------------------------------------------------------------------------
# Installation (headers and library)
# ---------------------------------------------------------------------------
if(JSON_QUERY_ENABLE_INSTALL)
  include(GNUInstallDirs)
  install(
    TARGETS json_query ctre_interface function_ref_interface
    EXPORT json_queryTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Export for downstream find_package(json_query)
  install(
    EXPORT json_queryTargets
    FILE json_queryTargets.cmake
    NAMESPACE json_query::
    DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)
endif()

# ---------------------------------------------------------------------------
# Refactoring tools (optional)
# ---------------------------------------------------------------------------
if(WITH_REFACTORING_TOOLS)
  add_subdirectory(refactoring)
endif()
