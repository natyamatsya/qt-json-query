# CMake configuration for the json_query library sources. This file is added by
# the top-level CMakeLists via `add_subdirectory`.

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(JSON_QUERY_SOURCES
    json-pointer/JSONPointer.cpp
    json-path/JSONPath.cpp
    json-path/JSONPathCreate.cpp
    json-path/JSONPathCompile.cpp
    json-path/JSONPathParseUtils.cpp
    json-path/JSONPathBracketRules.cpp
    json-path/JSONPathParsers.cpp
    json-path/JSONPathFilterHelpers.cpp
    json-path/JSONPathFilterComparison.cpp
    json-path/JSONPathFilterParsers.cpp
    json-path/JSONPathFilterFunctions.cpp
    json-path/JSONPathFilterCore.cpp
    json-path/JSONPathContextFilter.cpp
    json-path/JSONPathTokenEvaluators.cpp
    json-path/JSONPathPointerConversion.cpp
    json-path/internal/IterativeRecursiveDescent.cpp
    json-path/internal/CacheOptimizedStructures.cpp
    json-path/JSONPathEvaluate.cpp
    json-path/JSONPathEvalHelpers.cpp
    json-path/JSONPathTokenDispatch.cpp
    json-path/JSONPathWildcardRecursive.cpp
    json-path/JSONPathExpected.cpp
    json-path/JSONPathLog.cpp)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
add_library(json_query ${JSON_QUERY_SOURCES})

# Public include directories (header-only consumers) Use PROJECT_SOURCE_DIR to
# reference top-level include folder

target_include_directories(
  json_query
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/json-query>
         $<BUILD_INTERFACE:${ctre_SOURCE_DIR}/single-header>
         $<INSTALL_INTERFACE:include>)

# Link QtCore (Qt JSON classes live here)
target_link_libraries(json_query PUBLIC Qt6::Core)

# Compile out qDebug calls for performance optimization
target_compile_definitions(json_query PRIVATE QT_NO_DEBUG_OUTPUT)

# Link CTRE header-only interface when available
if(TARGET ctre_interface)
  target_link_libraries(json_query PRIVATE ctre_interface)
endif()

# Link function_ref header-only interface when available
if(TARGET function_ref_interface)
  target_link_libraries(json_query PRIVATE function_ref_interface)
endif()

# ---------------------------------------------------------------------------
# Installation (optional; relies on top-level JSON_QUERY_ENABLE_INSTALL option)
# ---------------------------------------------------------------------------
if(JSON_QUERY_ENABLE_INSTALL)
  install(
    TARGETS json_query
    EXPORT json_queryTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
