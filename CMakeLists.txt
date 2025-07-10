cmake_minimum_required(VERSION 3.24)

project(json_query VERSION 0.1 LANGUAGES CXX)

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
add_library(json_query
    src/JSONPointer.cpp
    src/JSONPath.cpp
)

# Public headers
target_include_directories(json_query
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link QtCore (Qt JSON classes live here)
# If additional Qt modules become necessary, add them here.
target_link_libraries(json_query PUBLIC Qt6::Core)

# ---------------------------------------------------------------------------
# Examples and tests
# ---------------------------------------------------------------------------
# Build example executables (simple_pointer, simple_path, ...)
add_subdirectory(examples)

# Unit-tests / benchmarks (requires Qt6::Test)
if (BUILD_TESTING)
    include(cmake/tests.cmake)
endif()

# ---------------------------------------------------------------------------
# Installation (headers and library)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS json_query EXPORT json_queryTargets
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
