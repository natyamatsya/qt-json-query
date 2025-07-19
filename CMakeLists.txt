cmake_minimum_required(VERSION 3.24)

project(json_query VERSION 0.1 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Testing option (enabled by default) and CTest include
# ---------------------------------------------------------------------------
option(BUILD_TESTING "Build and enable tests" ON)
include(CTest)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Allow the user to specify a Qt installation via CMAKE_PREFIX_PATH
# Example:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.3/macos

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
include(FetchContent)

# ---------------------------------------------------------------------------
# Header-only CTRE (compile-time regular expressions)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    ctre
    GIT_REPOSITORY https://github.com/hanickadot/compile-time-regular-expressions.git
    GIT_TAG main # any reasonably recent tag
    SOURCE_SUBDIR single-header
)
FetchContent_MakeAvailable(ctre)

# Create an interface target for the single-header library if none provided
if(NOT TARGET ctre_interface)
    add_library(ctre_interface INTERFACE)
    target_include_directories(ctre_interface INTERFACE ${ctre_SOURCE_DIR}/single-header)
endif()

add_library(json_query
        src/json-query/JSONPointer.cpp
        src/json-query/json-path/JSONPath.cpp
        src/json-query/json-path/JSONPathCreate.cpp
        src/json-query/json-path/JSONPathCompile.cpp
        src/json-query/json-path/JSONPathFilter.cpp
        src/json-query/json-path/JSONPathTokenEvaluators.cpp
        src/json-query/json-path/JSONPathEvaluate.cpp
        src/json-query/json-path/PathEvaluator.cpp
        src/json-query/json-path/JSONPathPointerConversion.cpp
)

# Public headers
target_include_directories(json_query
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/json-query>
        $<BUILD_INTERFACE:${ctre_SOURCE_DIR}/single-header>
        $<INSTALL_INTERFACE:include>
)

# Link QtCore (Qt JSON classes live here)
# If additional Qt modules become necessary, add them here.
target_link_libraries(json_query PUBLIC Qt6::Core)

# Link CTRE header-only library (interface target defined by FetchContent)
# link interface target
if(TARGET ctre_interface)
    target_link_libraries(json_query PRIVATE ctre_interface)
endif()

# ---------------------------------------------------------------------------
# Examples and tests
# ---------------------------------------------------------------------------
# Build example executables (simple_pointer, simple_path, ...)
add_subdirectory(examples)

# Unit-tests / benchmarks (requires Qt6::Test)
if (BUILD_TESTING)
    include(cmake/GoogleTests.cmake)
endif()

# Benchmarks
if (BUILD_BENCHMARKING)
    include(cmake/GoogleBenchmarks.cmake)
endif()

# ---------------------------------------------------------------------------
# Installation (headers and library)
# ---------------------------------------------------------------------------
if(JSON_QUERY_ENABLE_INSTALL)
    include(GNUInstallDirs)
    install(TARGETS json_query ctre_interface EXPORT json_queryTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # Export for downstream find_package(json_query)
    install(EXPORT json_queryTargets
            FILE json_queryTargets.cmake
            NAMESPACE json_query::
            DESTINATION ${CMAKE_INSTALL_DATADIR}/json_query/cmake)
endif()
