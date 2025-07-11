add_executable(simple_pointer simple_pointer.cpp)
target_link_libraries(simple_pointer PRIVATE json_query)

add_executable(simple_path simple_path.cpp)
target_link_libraries(simple_path PRIVATE json_query)

add_executable(complex_query complex_query.cpp)
target_link_libraries(complex_query PRIVATE json_query)

# New example comparing plain QJson vs json-query
add_executable(refactor_potential refactor_potential.cpp)
target_link_libraries(refactor_potential PRIVATE json_query)

install(TARGETS
    simple_pointer
    simple_path
    complex_query
    refactor_potential
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)
