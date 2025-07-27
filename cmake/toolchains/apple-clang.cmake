# Apple Clang Toolchain This toolchain file configures CMake to use Apple's
# Clang compiler

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Set the compiler
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Ensure we're using Apple Clang (not LLVM Clang)
execute_process(
  COMMAND ${CMAKE_CXX_COMPILER} --version
  OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT CLANG_VERSION_OUTPUT MATCHES "Apple clang")
  message(
    FATAL_ERROR
      "This toolchain requires Apple Clang, but found: ${CLANG_VERSION_OUTPUT}")
endif()

# Set compiler flags specific to Apple Clang
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_C_FLAGS_INIT "")

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apple-specific optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto=thin")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Set the toolchain identifier
set(CMAKE_TOOLCHAIN_ID "apple-clang")

message(STATUS "Using Apple Clang toolchain")
