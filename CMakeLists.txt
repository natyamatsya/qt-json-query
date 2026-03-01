cmake_minimum_required(VERSION 3.24)

project(
  json_query
  VERSION 0.2
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Testing option (enabled by default) and CTest include
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build and enable tests" ON)
option(BUILD_BENCHMARKING "Build and enable benchmarks" OFF)
include(CTest)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Refactoring tools option (disabled by default)
option(WITH_REFACTORING_TOOLS "Build C++ modernization refactoring tools" OFF)

# ---------------------------------------------------------------------------
# Sanitizer instrumentation for fuzz testing
# ---------------------------------------------------------------------------
# When fuzz testing is enabled, the entire project (including the library) must
# be compiled with ASan/UBSan so that fuzz targets can link against it.
option(ENABLE_FUZZ_TESTS "Enable LibFuzzer-based fuzz testing" OFF)
if(ENABLE_FUZZ_TESTS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Allow the user to specify a Qt installation via CMAKE_PREFIX_PATH Example:
# cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Network)

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

# ---------------------------------------------------------------------------
# Optional: SRELL (ECMAScript-compatible regex, header-only, BSD license)
# Enables ECMA-262 regex format validation with Unicode property escapes
# ---------------------------------------------------------------------------
option(JSON_QUERY_FORMAT_ECMA_REGEX
       "Enable ECMA-262 regex format validation (fetches SRELL)" OFF)

if(JSON_QUERY_FORMAT_ECMA_REGEX)
  FetchContent_Declare(
    srell URL https://www.akenotsuki.com/misc/srell/releases/srell4_140.zip
              DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(srell)

  if(NOT TARGET srell_interface)
    add_library(srell_interface INTERFACE)
    target_include_directories(srell_interface INTERFACE ${srell_SOURCE_DIR})
  endif()
endif()

# ---------------------------------------------------------------------------
# Optional: IDN hostname and IDN email format validation Backend choices:
# libidn2 - Full IDNA 2008 / RFC 5891+5892, LGPL-3.0 (system library) ada     -
# UTS #46 via ada-url/idna, Apache-2.0/MIT (fetched via FetchContent)
# ---------------------------------------------------------------------------
option(JSON_QUERY_FORMAT_IDN "Enable IDN hostname format validation" OFF)
set(JSON_QUERY_IDN_BACKEND
    "libidn2"
    CACHE
      STRING
      "IDN library backend: 'libidn2' (LGPL, system) or 'ada' (MIT, fetched)")
set_property(CACHE JSON_QUERY_IDN_BACKEND PROPERTY STRINGS libidn2 ada)

if(JSON_QUERY_FORMAT_IDN)
  if(JSON_QUERY_IDN_BACKEND STREQUAL "libidn2")
    # libidn2: full IDNA 2008 with contextual rules (RFC 5892) Install: brew
    # install libidn2 (macOS) or apt install libidn2-dev (Linux)
    find_path(IDN2_INCLUDE_DIR idn2.h)
    find_library(IDN2_LIBRARY NAMES idn2)

    if(IDN2_INCLUDE_DIR AND IDN2_LIBRARY)
      if(NOT TARGET idn2::idn2)
        add_library(idn2::idn2 UNKNOWN IMPORTED)
        set_target_properties(
          idn2::idn2
          PROPERTIES IMPORTED_LOCATION "${IDN2_LIBRARY}"
                     INTERFACE_INCLUDE_DIRECTORIES "${IDN2_INCLUDE_DIR}")
      endif()
      message(STATUS "IDN backend: libidn2 (${IDN2_LIBRARY})")
    else()
      message(
        FATAL_ERROR
          "libidn2 not found. Install: brew install libidn2 (macOS) / apt install libidn2-dev (Linux)"
      )
    endif()

  elseif(JSON_QUERY_IDN_BACKEND STREQUAL "ada")
    # ada-url/idna: UTS #46 implementation (lighter, permissive license)
    FetchContent_Declare(
      ada_idna
      GIT_REPOSITORY https://github.com/ada-url/idna.git
      GIT_TAG 0.4.0)

    set(ADA_IDNA_BENCHMARKS
        OFF
        CACHE BOOL "" FORCE)
    set(BUILD_TESTING_SAVED ${BUILD_TESTING})
    set(BUILD_TESTING
        OFF
        CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ada_idna)
    set(BUILD_TESTING
        ${BUILD_TESTING_SAVED}
        CACHE BOOL "" FORCE)
    message(STATUS "IDN backend: ada-url/idna (fetched)")

  else()
    message(
      FATAL_ERROR
        "Invalid JSON_QUERY_IDN_BACKEND: '${JSON_QUERY_IDN_BACKEND}'. Use 'libidn2' or 'ada'."
    )
  endif()
endif()

add_subdirectory(src/json-query)

# ---------------------------------------------------------------------------
# Examples and tests
# ---------------------------------------------------------------------------
option(JSON_QUERY_BUILD_EXAMPLES "Build example executables" ON)
option(JSON_QUERY_BUILD_PERF "Build performance test executables" ON)

if(JSON_QUERY_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(JSON_QUERY_BUILD_PERF)
  add_subdirectory(perf)
endif()

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
