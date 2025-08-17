# clang-cl Toolchain for Windows
# Configures CMake to use LLVM's clang-cl under MSVC-like environment

# Target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Locate clang-cl
find_program(CLANG_CL NAMES clang-cl)
if(NOT CLANG_CL)
  message(FATAL_ERROR "clang-cl not found. Install LLVM or Visual Studio with LLVM/Clang tools and ensure clang-cl.exe is in PATH.")
endif()

# Set compilers
set(CMAKE_C_COMPILER  "${CLANG_CL}")
set(CMAKE_CXX_COMPILER "${CLANG_CL}")

# Prefer lld-link if available
find_program(LLD_LINK NAMES lld-link)
if(LLD_LINK)
  set(CMAKE_LINKER "${LLD_LINK}")
endif()

# Use MSVC librarian (lib.exe) semantics with clang-cl.
# Do NOT override CMAKE_AR/CMAKE_RANLIB/CMAKE_NM to llvm-ar tools, because
# CMake will pass MSVC-style flags (/OUT, /nologo) for the librarian when using
# the MSVC-like clang-cl frontend. Overriding to llvm-ar causes flag mismatch.
find_program(LLVM_RC NAMES llvm-rc)
if(LLVM_RC)
  set(CMAKE_RC_COMPILER "${LLVM_RC}")
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Baseline flags similar to MSVC toolchain
set(CMAKE_CXX_FLAGS_INIT "/permissive- /Zc:__cplusplus /EHsc /bigobj")
set(CMAKE_C_FLAGS_INIT   "")

# Configuration-specific flags
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")

# Toolchain identifier
set(CMAKE_TOOLCHAIN_ID "clang-cl")
message(STATUS "Using clang-cl toolchain: ${CMAKE_CXX_COMPILER}")
