include(GoogleTest)

set(TESTS raptor/label_manager.cpp
        schedule/stop.cpp
        schedule/trip.cpp
        schedule/route.cpp
        transfers/kd_tree.cpp
        transfers/linear_walk_calculator.cpp
        transfers/transfers.cpp)

add_executable(pt_tests ${TESTS})
target_link_libraries(pt_tests GTest::gtest_main pt_routing)
gtest_discover_tests(pt_tests)