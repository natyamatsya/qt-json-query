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

# Example demonstrating the as<T> function
add_executable(as_function_demo as_function_demo.cpp)
target_link_libraries(as_function_demo PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(as_function_demo PRIVATE function_ref_interface)
endif()

add_executable(simple_schema simple_schema.cpp)
target_link_libraries(simple_schema PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(simple_schema PRIVATE function_ref_interface)
endif()

install(
  TARGETS simple_pointer
          monadic_error_handling_demo
          simple_path
          complex_query
          refactor_potential
          as_function_demo
          simple_schema
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
