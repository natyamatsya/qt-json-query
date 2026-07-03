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
    json-path/JSONPathTokenEvaluators.cpp
    json-path/JSONPathWildcardRecursive.cpp
    json-schema/JSONSchema.cpp
    json-schema/JSONSchemaCompile.cpp
    json-schema/JSONSchemaValidate.cpp
    json-schema/internal/FormatValidators.cpp
    json-schema/MetaSchema2020_12.cpp
    json-schema/SchemaRegistry.cpp)

# ---------------------------------------------------------------------------
# Library target
# ---------------------------------------------------------------------------
# Explicitly STATIC: the library has no symbol-visibility/export-macro story
# yet, so shared builds (in particular on Windows) are unsupported by design
# until v1.0 revisits this.
add_library(json_query STATIC ${JSON_QUERY_SOURCES})

# Static-library hygiene for shared-library consumers: hidden visibility keeps
# json_query symbols private to any .so/.dylib that embeds this archive, so two
# shared libraries each embedding json_query cannot interpose each other's
# copies. Complements the versioned inline ABI namespace (doc/adr/005).
set_target_properties(json_query PROPERTIES CXX_VISIBILITY_PRESET hidden
                                            VISIBILITY_INLINES_HIDDEN ON)

# Public include directories (header-only consumers) Use PROJECT_SOURCE_DIR to
# reference top-level include folder

target_include_directories(
  json_query
  PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/json-query>
         $<INSTALL_INTERFACE:include>)

# Link QtCore (Qt JSON classes live here) and QtNetwork (for QHostAddress)
target_link_libraries(json_query PUBLIC Qt6::Core Qt6::Network)

# Clang: Increase template instantiation depth for complex CTRE patterns Applied
# only to files with deeply nested regex - avoids affecting other TUs
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_source_files_properties(
    json-path/JSONPathParseUtils.cpp PROPERTIES COMPILE_OPTIONS
                                                "-ftemplate-depth=2000")
endif()

# MSVC: same mitigation - cl 19.51+ exceeds the default depth of 1000 on the
# slice-selector pattern (error C2999). Lives here (not in a preset toolchain
# file) so it also applies when the library is consumed via add_subdirectory.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set_source_files_properties(json-path/JSONPathParseUtils.cpp
                              PROPERTIES COMPILE_OPTIONS "/templateDepth:2000")
endif()

# Compile-time control over debug logging (category-based)
option(JSON_QUERY_ENABLE_DEBUG_LOGS "Enable debug logs in the library" OFF)
if(NOT JSON_QUERY_ENABLE_DEBUG_LOGS)
  # Strip qDebug/qCDebug at compile time for performance in non-debug builds
  target_compile_definitions(json_query PRIVATE QT_NO_DEBUG_OUTPUT)
endif()

# Link CTRE header-only interface. PRIVATE: since the JSONPath/JSONPointer
# pimpl, no header reachable from the public umbrella includes <ctre.hpp> —
# consumers do not need CTRE at all.
if(TARGET ctre_interface)
  # BUILD_INTERFACE: header-only — nothing to link downstream, keep the target
  # out of the install export requirements
  target_link_libraries(json_query PRIVATE $<BUILD_INTERFACE:ctre_interface>)
endif()

# Link function_ref header-only interface. PRIVATE: <tl/function_ref.hpp> is
# only used by internal headers, not the public umbrella surface.
if(TARGET function_ref_interface)
  target_link_libraries(json_query
                        PRIVATE $<BUILD_INTERFACE:function_ref_interface>)
endif()

# Optional: SRELL for ECMA-262 regex format validation (header-only —
# BUILD_INTERFACE keeps it out of the install export requirements)
if(TARGET srell_interface)
  target_link_libraries(json_query PRIVATE $<BUILD_INTERFACE:srell_interface>)
  target_compile_definitions(json_query PRIVATE JSON_QUERY_HAS_SRELL)
endif()

# Optional: IDN hostname format validation (backend-specific)
if(TARGET idn2::idn2)
  target_link_libraries(json_query PRIVATE idn2::idn2)
  target_compile_definitions(json_query PRIVATE JSON_QUERY_HAS_IDNA
                                                JSON_QUERY_IDN_LIBIDN2)
elseif(TARGET ada-idna)
  target_link_libraries(json_query PRIVATE ada-idna)
  target_compile_definitions(json_query PRIVATE JSON_QUERY_HAS_IDNA
                                                JSON_QUERY_IDN_ADA)
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
