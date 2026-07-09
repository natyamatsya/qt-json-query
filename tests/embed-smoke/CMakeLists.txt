# Standalone embed smoke test: NOT part of the main build. Proves json_query
# embeds cleanly via add_subdirectory: only the library target is configured (no
# test/example/perf/benchmark targets leak, no CTest side effects) and the
# json_query::json_query alias is available. Build and run: cmake -S
# tests/embed-smoke -B <bld> -DCMAKE_PREFIX_PATH="<qt>" cmake --build <bld>
# <bld>/embed_smoke
cmake_minimum_required(VERSION 3.24)
project(json_query_embed_smoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(_jq_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_subdirectory("${_jq_root}" json_query)

# Assert the PROJECT_IS_TOP_LEVEL defaults held: nothing beyond the library (and
# its dependency bridge targets) may have been configured.
foreach(_leaked json_query_core_tests simple_pointer memory_allocation_test
                json_benchmark consumer_smoke)
  if(TARGET ${_leaked})
    message(FATAL_ERROR "embedded json_query leaked target '${_leaked}'")
  endif()
endforeach()

if(NOT TARGET json_query::json_query)
  message(FATAL_ERROR "json_query::json_query alias missing")
endif()

# Reuse the consumer-smoke program: same API surface, different link path.
add_executable(embed_smoke ../consumer-smoke/main.cpp)
target_link_libraries(embed_smoke PRIVATE json_query::json_query)
