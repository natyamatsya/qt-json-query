# CMake configuration for the json_query library sources. This file is added by
# the top-level CMakeLists via `add_subdirectory`.

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(JSON_QUERY_SOURCES
    json-pointer/JSONPointer.cpp
    json-path/JSONPath.cpp
    json-path/JSONPathBracketRules.cpp
    json-path/JSONPathCompile.cpp
    json-path/JSONPathContextFilter.cpp
    json-path/JSONPathCreate.cpp
    json-path/JSONPathEvaluate.cpp
    json-path/JSONPathEvalHelpers.cpp
    json-path/JSONPathFilterComparison.cpp
    json-path/JSONPathFilterComparisonDispatcher.cpp
    json-path/JSONPathFilterCompile.cpp
    json-path/JSONPathFilterEmbeddedParsers.cpp
    json-path/JSONPathFilterExistenceParsers.cpp
    json-path/JSONPathFilterFunctions.cpp
    json-path/JSONPathFilterHelpers.cpp
    json-path/JSONPathFilterParsers.cpp
    json-path/JSONPathLog.cpp
    json-path/JSONPathParseUtils.cpp
    json-path/JSONPathParsers.cpp
    json-path/JSONPathPointerConversion.cpp
    json-path/JSONPathTokenDispatch.cpp
    json-path/JSONPathTokenEvaluators.cpp
    json-path/JSONPathWildcardRecursive.cpp
    json-path/internal/CacheOptimizedStructures.cpp
    json-path/internal/IterativeRecursiveDescent.cpp
    json-schema/JSONSchema.cpp
    json-schema/JSONSchemaCompile.cpp
    json-schema/JSONSchemaValidate.cpp)

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

# Compile-time control over debug logging (category-based)
option(JSON_QUERY_ENABLE_DEBUG_LOGS "Enable debug logs in the library" OFF)
if(NOT JSON_QUERY_ENABLE_DEBUG_LOGS)
  # Strip qDebug/qCDebug at compile time for performance in non-debug builds
  target_compile_definitions(json_query PRIVATE QT_NO_DEBUG_OUTPUT)
endif()

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
