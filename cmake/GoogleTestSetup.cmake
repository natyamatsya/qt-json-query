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

# Suppress -Wundef warnings from GoogleTest headers when using Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(gtest PRIVATE -Wno-undef)
  target_compile_options(gtest_main PRIVATE -Wno-undef)
  target_compile_options(gmock PRIVATE -Wno-undef)
  target_compile_options(gmock_main PRIVATE -Wno-undef)
endif()
