# CMakeLists.txt for compliance tools RFC 9535 compliance analysis and audit
# tools

# Error audit script for RFC 9535 compliance analysis
add_executable(error_audit_script src/error_audit_script.cpp)
target_link_libraries(error_audit_script PRIVATE json_query Qt6::Core)
target_include_directories(
  error_audit_script PRIVATE ${PROJECT_SOURCE_DIR}/include
                             ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(error_audit_script PRIVATE cxx_std_23)
if(TARGET function_ref_interface)
  target_link_libraries(error_audit_script PRIVATE function_ref_interface)
endif()

# CTS cross-reference script for detailed CTS compliance analysis
add_executable(cts_error_cross_reference src/cts_error_cross_reference.cpp)
target_link_libraries(cts_error_cross_reference PRIVATE json_query Qt6::Core)
target_include_directories(
  cts_error_cross_reference PRIVATE ${PROJECT_SOURCE_DIR}/include
                                    ${PROJECT_SOURCE_DIR}/include/json-query)
target_compile_features(cts_error_cross_reference PRIVATE cxx_std_23)
if(TARGET function_ref_interface)
  target_link_libraries(cts_error_cross_reference
                        PRIVATE function_ref_interface)
endif()

# RFC 9535 terminology audit script for terminology compliance analysis
# add_executable(rfc9535_terminology_audit src/rfc9535_terminology_audit.cpp)
# target_link_libraries(rfc9535_terminology_audit PRIVATE json_query Qt6::Core)
# target_include_directories( rfc9535_terminology_audit PRIVATE
# ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/include/json-query)
# target_co if(TARGET function_ref_interface)
# target_link_libraries(rfc9535_terminology_audit PRIVATE
# function_ref_interface) endif()
