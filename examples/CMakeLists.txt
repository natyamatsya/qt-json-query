add_executable(simple_pointer simple_pointer.cpp)
target_link_libraries(simple_pointer PRIVATE json_query)

add_executable(compilation_time_passes_demo compilation_time_passes_demo.cpp)
target_link_libraries(compilation_time_passes_demo json_query Qt6::Core)

add_executable(pass_pipeline_demo pass_pipeline_demo.cpp)
target_link_libraries(pass_pipeline_demo json_query Qt6::Core)

add_executable(monadic_error_handling_demo monadic_error_handling_demo.cpp)
target_link_libraries(monadic_error_handling_demo json_query Qt6::Core)

add_executable(simple_path simple_path.cpp)
target_link_libraries(simple_path PRIVATE json_query)

add_executable(complex_query complex_query.cpp)
target_link_libraries(complex_query PRIVATE json_query)

# New example comparing plain QJson vs json-query
add_executable(refactor_potential refactor_potential.cpp)
target_link_libraries(refactor_potential PRIVATE json_query)

install(TARGETS
    simple_pointer
    compilation_time_passes_demo
    pass_pipeline_demo
    monadic_error_handling_demo
    simple_path
    complex_query
    refactor_potential
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)
