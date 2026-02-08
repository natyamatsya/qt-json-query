add_executable(simple_pointer simple_pointer.cpp)
target_link_libraries(simple_pointer PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(simple_pointer PRIVATE function_ref_interface)
endif()

add_executable(simple_path simple_path.cpp)
target_link_libraries(simple_path PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(simple_path PRIVATE function_ref_interface)
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

add_executable(flagship_pipeline flagship_pipeline.cpp)
target_link_libraries(flagship_pipeline PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(flagship_pipeline PRIVATE function_ref_interface)
endif()

add_executable(compile_once_reuse compile_once_reuse.cpp)
target_link_libraries(compile_once_reuse PRIVATE json_query Qt6::Core)
if(TARGET function_ref_interface)
  target_link_libraries(compile_once_reuse PRIVATE function_ref_interface)
endif()

install(
  TARGETS simple_pointer
          simple_path
          refactor_potential
          as_function_demo
          simple_schema
          flagship_pipeline
          compile_once_reuse
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)
