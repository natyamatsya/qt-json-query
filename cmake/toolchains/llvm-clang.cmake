# LLVM Clang Toolchain This toolchain file configures CMake to use LLVM Clang
# compiler

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Try to find LLVM Clang in common locations
find_program(
  LLVM_CLANG_CXX
  NAMES clang++
  PATHS /usr/local/opt/llvm/bin /opt/homebrew/opt/llvm/bin /usr/local/bin
        /opt/local/bin
  NO_DEFAULT_PATH)

find_program(
  LLVM_CLANG_C
  NAMES clang
  PATHS /usr/local/opt/llvm/bin /opt/homebrew/opt/llvm/bin /usr/local/bin
        /opt/local/bin
  NO_DEFAULT_PATH)

# Fallback to system clang if LLVM version not found
if(NOT LLVM_CLANG_CXX)
  find_program(LLVM_CLANG_CXX clang++)
endif()

if(NOT LLVM_CLANG_C)
  find_program(LLVM_CLANG_C clang)
endif()

if(NOT LLVM_CLANG_CXX OR NOT LLVM_CLANG_C)
  message(
    FATAL_ERROR
      "LLVM Clang not found. Please install LLVM or set CMAKE_CXX_COMPILER manually."
  )
endif()

# Set the compiler
set(CMAKE_C_COMPILER ${LLVM_CLANG_C})
set(CMAKE_CXX_COMPILER ${LLVM_CLANG_CXX})

# Force use of system archiver and ranlib to avoid LLVM archiver compatibility
# issues on macOS
set(CMAKE_AR /usr/bin/ar)
set(CMAKE_RANLIB /usr/bin/ranlib)
set(CMAKE_NM /usr/bin/nm)

# Verify we're using LLVM Clang (not Apple Clang)
execute_process(
  COMMAND ${CMAKE_CXX_COMPILER} --version
  OUTPUT_VARIABLE CLANG_VERSION_OUTPUT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CLANG_VERSION_OUTPUT MATCHES "Apple clang")
  message(
    WARNING "Found Apple Clang instead of LLVM Clang: ${CLANG_VERSION_OUTPUT}")
  message(WARNING "Consider installing LLVM via Homebrew: brew install llvm")
endif()

# Set compiler flags specific to LLVM Clang
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_C_FLAGS_INIT "")

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# LLVM-specific optimizations
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Enable additional LLVM-specific features
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcolor-diagnostics")

# Set the toolchain identifier
set(CMAKE_TOOLCHAIN_ID "llvm-clang")

message(STATUS "Using LLVM Clang toolchain: ${CMAKE_CXX_COMPILER}")
