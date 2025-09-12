include(GoogleTest)

set(TESTS raptor/label_manager.cpp)

add_executable(pt_tests ${TESTS})
target_link_libraries(pt_tests GTest::gtest_main pt_routing_base)
gtest_discover_tests(pt_tests)