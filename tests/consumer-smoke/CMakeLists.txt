# Standalone consumer smoke test: NOT part of the main build. Built by CI (and
# locally) against an installed prefix to prove the find_package flow works:
#   cmake --install <build> --prefix <prefix>
#   cmake -S tests/consumer-smoke -B <bld> -DCMAKE_PREFIX_PATH="<qt>;<prefix>"
cmake_minimum_required(VERSION 3.24)
project(json_query_consumer_smoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(json_query REQUIRED)

add_executable(consumer_smoke main.cpp)
target_link_libraries(consumer_smoke PRIVATE json_query::json_query)
