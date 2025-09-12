include(GoogleTest)

set(TESTS raptor/label_manager.cpp)

add_executable(tests ${TESTS})
target_link_libraries(tests GTest::gtest_main pt_routing_base)
gtest_discover_tests(tests)