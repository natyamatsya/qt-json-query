# CMakeLists.txt for performance analysis tools
# Memory optimization, profiling, and performance testing tools

# Memory allocation test for tracking allocations
add_executable(memory_allocation_test src/memory_allocation_test.cpp)
target_link_libraries(memory_allocation_test PRIVATE json_query Qt6::Core)
target_include_directories(memory_allocation_test PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/json-query
)
target_compile_features(memory_allocation_test PRIVATE cxx_std_23)

# Memory optimization test for validating optimization strategies
add_executable(memory_optimization_test src/memory_optimization_test.cpp)
target_link_libraries(memory_optimization_test PRIVATE json_query Qt6::Core)
target_include_directories(memory_optimization_test PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/json-query
)
target_compile_features(memory_optimization_test PRIVATE cxx_std_23)
if(TARGET function_ref_interface)
    target_link_libraries(memory_optimization_test PRIVATE function_ref_interface)
endif()

# Qt memory tracker for Qt-specific memory analysis
add_executable(qt_memory_tracker src/qt_memory_tracker.cpp)
target_link_libraries(qt_memory_tracker PRIVATE json_query Qt6::Core)
target_include_directories(qt_memory_tracker PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/json-query
)
target_compile_features(qt_memory_tracker PRIVATE cxx_std_23)

# Allocation hotspot analyzer for identifying performance bottlenecks
add_executable(allocation_hotspot_analyzer src/allocation_hotspot_analyzer.cpp)
target_link_libraries(allocation_hotspot_analyzer PRIVATE json_query Qt6::Core)
target_include_directories(allocation_hotspot_analyzer PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/json-query
)
target_compile_features(allocation_hotspot_analyzer PRIVATE cxx_std_23)

# Profiling test for performance analysis
add_executable(profile_test src/profile_test.cpp)
target_link_libraries(profile_test PRIVATE json_query Qt6::Core)
target_include_directories(profile_test PRIVATE 
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include/json-query
)
target_compile_features(profile_test PRIVATE cxx_std_23)

# Include gperftools for profiling if available
find_path(GPERFTOOLS_INCLUDE_DIR gperftools/profiler.h HINTS /opt/homebrew/include)
find_library(GPERFTOOLS_PROFILER_LIB profiler HINTS /opt/homebrew/lib)

if(GPERFTOOLS_INCLUDE_DIR AND GPERFTOOLS_PROFILER_LIB)
    target_include_directories(profile_test PRIVATE ${GPERFTOOLS_INCLUDE_DIR})
    target_link_libraries(profile_test PRIVATE ${GPERFTOOLS_PROFILER_LIB})
    target_compile_definitions(profile_test PRIVATE HAVE_GPERFTOOLS)
endif()
