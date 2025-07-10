# Enable testing
enable_testing()

# Tests requiring QtTest (optional)
find_package(Qt6 COMPONENTS Test QUIET)

# If QtTest is not available, skip adding tests to avoid configure failure
if (NOT Qt6Test_FOUND)
    message(STATUS "Qt6::Test not found; skipping unit tests.")
    return()
endif()

# Add test executables
add_executable(jsonpointer_test ${PROJECT_SOURCE_DIR}/tests/JSONPointerTest.cpp)
# Enable Qt MOC
set_target_properties(jsonpointer_test PROPERTIES AUTOMOC ON)
# Include project headers
target_include_directories(jsonpointer_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(jsonpointer_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test
)

add_executable(jsonpath_test ${PROJECT_SOURCE_DIR}/tests/JSONPathTest.cpp)
set_target_properties(jsonpath_test PROPERTIES AUTOMOC ON)
target_include_directories(jsonpath_test PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(jsonpath_test PRIVATE 
    json_query 
    Qt6::Core 
    Qt6::Test
)


# Register tests with CTest
add_test(NAME JSONPointerTest COMMAND jsonpointer_test)
add_test(NAME JSONPathTest COMMAND jsonpath_test)
# Set test properties
set_tests_properties(JSONPointerTest PROPERTIES TIMEOUT 60)
set_tests_properties(JSONPathTest PROPERTIES TIMEOUT 60)
