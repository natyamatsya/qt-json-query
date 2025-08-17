# GCC Toolchain - Configures CMake to use GCC on Linux (and other Unix-like hosts)

# Default target to host OS/arch unless cross-compiling
if(NOT DEFINED CMAKE_SYSTEM_NAME)
  set(CMAKE_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_NAME})
endif()
if(NOT DEFINED CMAKE_SYSTEM_PROCESSOR)
  set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
endif()

# Locate GCC compilers
find_program(GCC_CXX NAMES g++ c++ PATHS /usr/bin /usr/local/bin /bin)
find_program(GCC_C NAMES gcc cc PATHS /usr/bin /usr/local/bin /bin)

if(NOT GCC_CXX OR NOT GCC_C)
  message(FATAL_ERROR "GCC not found. Please install GCC or set CMAKE_CXX_COMPILER/CMAKE_C_COMPILER manually.")
endif()

# Set the compilers
set(CMAKE_C_COMPILER ${GCC_C})
set(CMAKE_CXX_COMPILER ${GCC_CXX})

# Prefer lld or gold if available for faster linking
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_program(LLD_LINKER NAMES ld.lld lld)
  if(LLD_LINKER)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
  else()
    find_program(GOLD_LINKER NAMES ld.gold)
    if(GOLD_LINKER)
      set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=gold")
      set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=gold")
    endif()
  endif()
endif()

# GCC diagnostics coloring
set(CMAKE_CXX_FLAGS_INIT "-fdiagnostics-color=always")
set(CMAKE_C_FLAGS_INIT "-fdiagnostics-color=always")

# Language standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build-type flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")

# Toolchain identifier
set(CMAKE_TOOLCHAIN_ID "gcc")

message(STATUS "Using GCC toolchain: ${CMAKE_CXX_COMPILER}")
