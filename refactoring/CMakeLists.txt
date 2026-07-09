# CMakeLists.txt for Refactoring Tools Optional compilation of C++ modernization
# tools used during development
#
# Only entered when the top-level WITH_REFACTORING_TOOLS option is ON (the
# option lives in the top-level CMakeLists.txt, which guards this
# add_subdirectory).

message(STATUS "Building refactoring tools...")

# Set C++ standard for refactoring tools
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags for refactoring tools
set(REFACTORING_CXX_FLAGS -Wall -Wextra -O2 -std=c++17)

# Auto Modernization Tool
add_executable(comprehensive_auto_modernizer
               src/comprehensive_auto_modernizer.cpp)
target_compile_options(comprehensive_auto_modernizer
                       PRIVATE ${REFACTORING_CXX_FLAGS})
set_target_properties(
  comprehensive_auto_modernizer
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts")

# Brace Initialization Modernization Tool
add_executable(comprehensive_brace_modernizer
               src/comprehensive_brace_modernizer.cpp)
target_compile_options(comprehensive_brace_modernizer
                       PRIVATE ${REFACTORING_CXX_FLAGS})
set_target_properties(
  comprehensive_brace_modernizer
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts")

# Variable Assignment Modernization Tool
add_executable(comprehensive_variable_modernizer
               src/comprehensive_variable_modernizer.cpp)
target_compile_options(comprehensive_variable_modernizer
                       PRIVATE ${REFACTORING_CXX_FLAGS})
set_target_properties(
  comprehensive_variable_modernizer
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scripts")

# Reference Auto Modernization Tool
add_executable(reference_auto_modernizer src/reference_auto_modernizer.cpp)
target_compile_options(reference_auto_modernizer
                       PRIVATE ${REFACTORING_CXX_FLAGS})
set_target_properties(
  reference_auto_modernizer PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                       "${CMAKE_CURRENT_SOURCE_DIR}/scripts")

# Create a target to build all refactoring tools
add_custom_target(
  refactoring_tools
  DEPENDS comprehensive_auto_modernizer comprehensive_brace_modernizer
          comprehensive_variable_modernizer reference_auto_modernizer
  COMMENT "Building all refactoring tools")

# Installation (optional)
if(CMAKE_INSTALL_PREFIX)
  install(TARGETS comprehensive_auto_modernizer comprehensive_brace_modernizer
                  comprehensive_variable_modernizer reference_auto_modernizer
          RUNTIME DESTINATION bin/refactoring COMPONENT refactoring_tools)

  # Install scripts
  install(
    FILES scripts/diagnostic_auto_modernization.sh
          scripts/diagnostic_brace_modernization.sh
          scripts/diagnostic_reference_auto_modernization.sh
          scripts/working_clang_tidy.sh
    DESTINATION bin/refactoring
    PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_EXECUTE
      WORLD_READ
      WORLD_EXECUTE
    COMPONENT refactoring_tools)
endif()

message(
  STATUS
    "Refactoring tools configured. Build with: cmake --build . --target refactoring_tools"
)
message(STATUS "Tools will be output to: ${CMAKE_CURRENT_SOURCE_DIR}/scripts/")
