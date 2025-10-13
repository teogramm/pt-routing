cmake_minimum_required(VERSION 3.14)

project(pt_routing_tests LANGUAGES CXX)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

set(TESTS raptor/label_manager.cpp
        schedule/stop.cpp
        schedule/trip.cpp
        schedule/route.cpp
        transfers/kd_tree.cpp
        transfers/linear_walk_calculator.cpp
        transfers/transfers.cpp)

add_executable(pt_tests ${TESTS})
target_link_libraries(pt_tests GTest::gtest_main pt_routing)

include(GoogleTest)
gtest_discover_tests(pt_tests)