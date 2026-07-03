# Basic GoogleTest setup (fetch and make targets available) This file is meant
# to be included by the top-level CMakeLists.txt BEFORE any test subdirectories
# are added.  It fetches the GoogleTest source and exposes the standard targets
# (GTest::gtest, GTest::gmock, etc.).

# Ensure testing is enabled
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
set(gtest_force_shared_crt
    ON
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
  foreach(gtest_target gtest gtest_main gmock gmock_main)
    target_compile_options(${gtest_target} PRIVATE ${_gtest_warning_opts})
  endforeach()
  unset(_gtest_warning_opts)
endif()
