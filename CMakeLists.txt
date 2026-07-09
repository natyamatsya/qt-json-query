cmake_minimum_required(VERSION 3.24)

# NOTE: when bumping <major>.<minor>, also bump JSON_QUERY_ABI_NS in
# include/json-query/config/AbiNamespace.hpp (see doc/adr/005)
project(
  json_query
  VERSION 0.5.0
  LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Testing option (enabled by default) and CTest include
# ---------------------------------------------------------------------------
# BUILD_TESTING (default ON) is declared by the CTest module itself
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

# Install/export rules (on for top-level builds, off when embedded)
option(JSON_QUERY_ENABLE_INSTALL "Generate install/export rules for json_query"
       ${PROJECT_IS_TOP_LEVEL})

# ---------------------------------------------------------------------------
# Sanitizer instrumentation
# ---------------------------------------------------------------------------
# JSON_QUERY_ENABLE_SANITIZERS builds everything with ASan+UBSan (standalone
# memory-safety validation, e.g. the CI sanitizer job). When fuzz testing is
# enabled, the entire project (including the library) must also be compiled with
# ASan/UBSan so that fuzz targets can link against it.
option(JSON_QUERY_ENABLE_SANITIZERS
       "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_FUZZ_TESTS "Enable LibFuzzer-based fuzz testing" OFF)
if(JSON_QUERY_ENABLE_SANITIZERS OR ENABLE_FUZZ_TESTS)
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
#
# Resolution order (package-manager friendly, e.g. vcpkg): 1. find_package(ctre
# CONFIG) — satisfied by vcpkg's `ctre` port or any system install, letting
# consumers control the version 2. FetchContent fallback pinned below Consumers
# can also override the fetched version by calling FetchContent_Declare(ctre
# ...) before add_subdirectory of this project (first declaration wins), or by
# setting FETCHCONTENT_SOURCE_DIR_CTRE.
# ---------------------------------------------------------------------------
FetchContent_Declare(
  ctre
  GIT_REPOSITORY
    https://github.com/hanickadot/compile-time-regular-expressions.git
  # v3.10.0 + 2 commits (includes a GCC warning fix); pinned for reproducible
  # builds — CTRE is a public compile-time dependency baked into our headers
  GIT_TAG 6225211806c48230e5d17a1e555ef69e7325051c
  SOURCE_SUBDIR single-header FIND_PACKAGE_ARGS NAMES ctre)
FetchContent_MakeAvailable(ctre)

# Bridge target used throughout the build: forwards to the package target when
# find_package satisfied the dependency, to the fetched tree otherwise.
if(NOT TARGET ctre_interface)
  add_library(ctre_interface INTERFACE)
  if(TARGET ctre::ctre)
    target_link_libraries(ctre_interface INTERFACE ctre::ctre)
  else()
    target_include_directories(
      ctre_interface
      INTERFACE $<BUILD_INTERFACE:${ctre_SOURCE_DIR}/single-header>)
  endif()
endif()

# ---------------------------------------------------------------------------
# Header-only function_ref (lightweight non-owning function reference)
#
# Resolution order: find_package(tl-function-ref CONFIG) first (vcpkg's
# `tl-function-ref` port provides tl::function_ref), FetchContent fallback.
# Override via an earlier FetchContent_Declare(function_ref ...) or
# FETCHCONTENT_SOURCE_DIR_FUNCTION_REF.
# ---------------------------------------------------------------------------
# SOURCE_SUBDIR include: the include/ directory has no CMakeLists.txt, so
# FetchContent_MakeAvailable only populates the sources without executing
# upstream's project files (v1.0.0's CMakeLists fetches tl_cmake via the
# deprecated FetchContent_Populate and warns on every configure). We only need
# the header; the bridge target below wires up the include dir.
FetchContent_Declare(
  function_ref
  GIT_REPOSITORY https://github.com/TartanLlama/function_ref.git
  GIT_TAG v1.0.0
  SOURCE_SUBDIR include FIND_PACKAGE_ARGS NAMES tl-function-ref)

FetchContent_MakeAvailable(function_ref)

# Bridge target: tl::function_ref exists both when the package was found and
# when the fetched project defined it; fall back to a raw include dir.
if(NOT TARGET function_ref_interface)
  add_library(function_ref_interface INTERFACE)
  if(TARGET tl::function_ref)
    target_link_libraries(function_ref_interface INTERFACE tl::function_ref)
  else()
    target_include_directories(
      function_ref_interface
      INTERFACE $<BUILD_INTERFACE:${function_ref_SOURCE_DIR}/include>)
  endif()
endif()

# ---------------------------------------------------------------------------
# Optional: SRELL (ECMAScript-compatible regex, header-only, BSD license)
# Enables ECMA-262 regex format validation with Unicode property escapes
# ---------------------------------------------------------------------------
option(JSON_QUERY_FORMAT_ECMA_REGEX
       "Enable ECMA-262 regex format validation (fetches SRELL)" OFF)

# SRELL has no official vcpkg port; consumers can point this at their own copy
# (e.g. a vcpkg overlay port) to skip the download.
set(JSON_QUERY_SRELL_INCLUDE_DIR
    ""
    CACHE PATH "Directory containing srell.hpp (overrides the download)")

if(JSON_QUERY_FORMAT_ECMA_REGEX)
  if(JSON_QUERY_SRELL_INCLUDE_DIR)
    if(NOT TARGET srell_interface)
      add_library(srell_interface INTERFACE)
      target_include_directories(srell_interface
                                 INTERFACE ${JSON_QUERY_SRELL_INCLUDE_DIR})
    endif()
    message(STATUS "SRELL: using ${JSON_QUERY_SRELL_INCLUDE_DIR}")
  else()
    FetchContent_Declare(
      srell
      URL https://www.akenotsuki.com/misc/srell/releases/srell4_140.zip
      URL_HASH
        SHA256=c2cd1bc13a01e4518be59d247287e72ce1f9d697ae82fcf26320bf6f35821cb8
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(srell)

    if(NOT TARGET srell_interface)
      add_library(srell_interface INTERFACE)
      target_include_directories(srell_interface INTERFACE ${srell_SOURCE_DIR})
    endif()
  endif()
endif()

# ---------------------------------------------------------------------------
# Optional: IDN hostname and IDN email format validation Backend choices: ada -
# UTS #46 via ada-url/idna, Apache-2.0/MIT (fetched via FetchContent) libidn2 -
# Full IDNA 2008 / RFC 5891+5892, LGPL-3.0 (system library)
#
# The default is the permissively licensed ada backend: this library is
# static-only, so an LGPL dependency imposes relink obligations on every
# consumer binary (LGPL-3.0 s4(d)). libidn2 stays available for its fuller RFC
# 5892 coverage, but only as an explicit opt-in.
# ---------------------------------------------------------------------------
option(JSON_QUERY_FORMAT_IDN "Enable IDN hostname format validation" OFF)
set(JSON_QUERY_IDN_BACKEND
    "ada"
    CACHE
      STRING
      "IDN library backend: 'ada' (Apache-2.0/MIT, fetched) or 'libidn2' (LGPL-3.0, system, explicit opt-in)"
)
set_property(CACHE JSON_QUERY_IDN_BACKEND PROPERTY STRINGS ada libidn2)

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
      message(
        STATUS
          "IDN backend: libidn2 (${IDN2_LIBRARY}) -- LGPL-3.0: statically linking consumers must honor its relink obligations"
      )
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
    # ada-idna exports /WX /W3 /sdl as INTERFACE compile options on MSVC,
    # forcing warnings-as-errors onto every consuming translation unit; keep its
    # build flags out of ours.
    if(TARGET ada-idna)
      set_target_properties(ada-idna PROPERTIES INTERFACE_COMPILE_OPTIONS "")
    endif()
    message(STATUS "IDN backend: ada-url/idna (fetched)")

  else()
    message(
      FATAL_ERROR
        "Invalid JSON_QUERY_IDN_BACKEND: '${JSON_QUERY_IDN_BACKEND}'. Use 'ada' (default, permissive) or 'libidn2' (LGPL-3.0)."
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
  # json_query itself is added to the export set in
  # src/json-query/CMakeLists.txt. CTRE/function_ref are PRIVATE build
  # dependencies (not part of the public header surface since the pimpl) and are
  # deliberately NOT exported.
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Export for downstream find_package(json_query)
  install(
    EXPORT json_queryTargets
    FILE json_queryTargets.cmake
    NAMESPACE json_query::
    DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)

  # Package config + version file so find_package(json_query) works
  include(CMakePackageConfigHelpers)
  configure_package_config_file(
    cmake/json_queryConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/json_queryConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)
  # Pre-1.0: minor versions may break API, so compatibility is SameMinorVersion
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/json_queryConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/json_queryConfig.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/json_queryConfigVersion.cmake
          DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)

  if(JSON_QUERY_FORMAT_IDN AND JSON_QUERY_IDN_BACKEND STREQUAL "ada")
    message(
      WARNING
        "JSON_QUERY_ENABLE_INSTALL with the fetched 'ada' IDN backend is not "
        "supported (the ada-idna target is not exported); use the libidn2 "
        "backend for installed builds.")
  endif()

  # -------------------------------------------------------------------------
  # SBOM generation (optional; SPDX 3.0.1 JSON-LD via CMake's native
  # install(SBOM), added in CMake 4.3 as an experimental feature)
  # -------------------------------------------------------------------------
  option(JSON_QUERY_ENABLE_SBOM
         "Install an SPDX SBOM generated from the export set (CMake 4.3+)" OFF)

  if(JSON_QUERY_ENABLE_SBOM)
    if(CMAKE_VERSION VERSION_LESS 4.3)
      message(
        FATAL_ERROR
          "JSON_QUERY_ENABLE_SBOM requires CMake 4.3+ for install(SBOM); "
          "this is CMake ${CMAKE_VERSION}")
    endif()

    # Opt in to the experimental feature. The UUID is CMake-release-specific and
    # must be updated when upgrading CMake until the feature is stable (see
    # Help/dev/experimental.rst in the CMake sources; this value is for the
    # CMake 4.3 series).
    set(CMAKE_EXPERIMENTAL_GENERATE_SBOM "ca494ed3-b261-4205-a01f-603c95e4cae0")

    install(
      SBOM
      json_query
      EXPORT
      json_queryTargets
      VERSION
      ${PROJECT_VERSION}
      LICENSE
      "Apache-2.0 WITH LLVM-exception OR MIT"
      DESCRIPTION
      "JSON Pointer (RFC 6901), JSONPath (RFC 9535), and JSON Schema (Draft 2020-12) for Qt"
      HOMEPAGE_URL
      "https://github.com/natyamatsya/qt-json-query")
  endif()
endif()

# ---------------------------------------------------------------------------
# Refactoring tools (optional)
# ---------------------------------------------------------------------------
if(WITH_REFACTORING_TOOLS)
  add_subdirectory(refactoring)
endif()
