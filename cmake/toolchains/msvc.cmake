# MSVC Toolchain
# This toolchain file configures CMake to use Microsoft Visual C++ (MSVC)

# Target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Use MSVC compiler (requires Developer Command Prompt / VsDevCmd environment)
set(CMAKE_C_COMPILER cl)
set(CMAKE_CXX_COMPILER cl)

# Ensure we're using MSVC
if(NOT MSVC)
  message(FATAL_ERROR "This toolchain requires MSVC (cl.exe) to be available in PATH.")
endif()

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Baseline MSVC flags
# - /permissive- for strict standard conformance
# - /Zc:__cplusplus to report the correct __cplusplus value
# - /Zc:preprocessor for standard-compliant preprocessor
# - /EHsc for C++ exceptions
set(CMAKE_CXX_FLAGS_INIT "/permissive- /Zc:__cplusplus /Zc:preprocessor /EHsc")
set(CMAKE_C_FLAGS_INIT "/Zc:preprocessor")

# Configuration-specific optimizations (rely mostly on CMake defaults, add clarity)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")

# Toolchain identifier
set(CMAKE_TOOLCHAIN_ID "msvc")

message(STATUS "Using MSVC toolchain (cl.exe)")
