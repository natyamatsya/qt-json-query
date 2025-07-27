add_executable(simple_pointer simple_pointer.cpp)
target_link_libraries(simple_pointer PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(simple_pointer PRIVATE function_ref_interface)
endif()

add_executable(monadic_error_handling_demo monadic_error_handling_demo.cpp)
target_link_libraries(monadic_error_handling_demo PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(monadic_error_handling_demo
                        PRIVATE function_ref_interface)
endif()

add_executable(simple_path simple_path.cpp)
target_link_libraries(simple_path PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(simple_path PRIVATE function_ref_interface)
endif()

add_executable(complex_query complex_query.cpp)
target_link_libraries(complex_query PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(complex_query PRIVATE function_ref_interface)
endif()

add_executable(refactor_potential refactor_potential.cpp)
target_link_libraries(refactor_potential PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(refactor_potential PRIVATE function_ref_interface)
endif()

# function_ref demonstration and performance comparison
add_executable(function_ref_demo function_ref_demo.cpp)
target_link_libraries(function_ref_demo PRIVATE json_query)
if(TARGET function_ref_interface)
  target_link_libraries(function_ref_demo PRIVATE function_ref_interface)
endif()

install(TARGETS simple_pointer monadic_error_handling_demo simple_path
                complex_query refactor_potential function_ref_demo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
