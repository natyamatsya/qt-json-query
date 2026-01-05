# MSVC Toolchain This toolchain file configures CMake to use Microsoft Visual
# C++ (MSVC)

# Target platform
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Locate MSVC compiler (requires Developer Command Prompt / VsDevCmd
# environment)
find_program(MSVC_CL NAMES cl.exe cl)
if(NOT MSVC_CL)
  message(
    FATAL_ERROR
      "MSVC compiler 'cl.exe' not found in PATH. Ensure you are using a Developer Command Prompt/PowerShell."
  )
endif()
set(CMAKE_C_COMPILER "${MSVC_CL}")
set(CMAKE_CXX_COMPILER "${MSVC_CL}")

# Enable modern C++ features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Baseline MSVC flags - /permissive- for strict standard conformance -
# /Zc:__cplusplus to report the correct __cplusplus value - /Zc:preprocessor for
# standard-compliant preprocessor - /EHsc for C++ exceptions
set(CMAKE_CXX_FLAGS_INIT "/permissive- /Zc:__cplusplus /Zc:preprocessor /EHsc")
set(CMAKE_C_FLAGS_INIT "/Zc:preprocessor")

# Configuration-specific optimizations (rely mostly on CMake defaults, add
# clarity)
set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Zi /DNDEBUG")

# Toolchain identifier
set(CMAKE_TOOLCHAIN_ID "msvc")

message(STATUS "Using MSVC toolchain (cl.exe)")
