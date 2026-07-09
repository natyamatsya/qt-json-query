# Basic GoogleTest setup (fetch and make targets available) This file is meant
# to be included by the top-level CMakeLists.txt BEFORE any test subdirectories
# are added.  It fetches the GoogleTest source and exposes the standard targets
# (GTest::gtest, GTest::gmock, etc.).

# Ensure testing is enabled
enable_testing()

include(FetchContent)
# FIND_PACKAGE_ARGS: prefer a package-manager copy (vcpkg's `gtest` port, a
# system GTest, or a superbuild's) over the pinned fetch; the fetch is only a
# fallback. All test modules link the namespaced GTest:: targets, which both
# resolution paths provide.
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
      FIND_PACKAGE_ARGS 1.14 NAMES GTest)
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
# Keep the fetched GoogleTest out of our install rules (it would otherwise land
# in the install prefix alongside json_query)
set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Suppress warnings from GoogleTest's own strict build when using Clang: -Wundef
# (config macros) and -Wcharacter-conversion (char8_t in gtest-printers.h, first
# diagnosed by clang 21). The latter group does not exist in older
# clangs/AppleClang, where the -Wno- form would itself raise
# -Wunknown-warning-option whenever another diagnostic fires — so probe it.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(CheckCXXCompilerFlag)
  # Clang accepts unknown -W flags with only a warning; promote it to an error
  # so the probe actually fails on unsupported groups.
  set(CMAKE_REQUIRED_FLAGS "-Werror=unknown-warning-option")
  check_cxx_compiler_flag(-Wcharacter-conversion
                          JSON_QUERY_HAVE_WCHARACTER_CONVERSION)
  unset(CMAKE_REQUIRED_FLAGS)

  set(_gtest_warning_opts -Wno-undef)
  if(JSON_QUERY_HAVE_WCHARACTER_CONVERSION)
    list(APPEND _gtest_warning_opts -Wno-character-conversion)
  endif()
  # The bare build targets only exist when FetchContent built GoogleTest from
  # source; skip when find_package satisfied the dependency with imported
  # targets (their warnings are not our build's problem).
  foreach(gtest_target gtest gtest_main gmock gmock_main)
    if(TARGET ${gtest_target})
      target_compile_options(${gtest_target} PRIVATE ${_gtest_warning_opts})
    endif()
  endforeach()
  unset(_gtest_warning_opts)
endif()
