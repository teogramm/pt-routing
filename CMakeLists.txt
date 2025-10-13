cmake_minimum_required(VERSION 3.31)
project(pt_routing)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost REQUIRED COMPONENTS graph)

include(FetchContent)
FetchContent_Declare(
        just_gtfs
        GIT_REPOSITORY https://github.com/mapsme/just_gtfs
        EXCLUDE_FROM_ALL
)

set(NANOFLANN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
        nanoflann
        GIT_REPOSITORY https://github.com/jlblancoc/nanoflann
        GIT_TAG v1.7.1
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(just_gtfs nanoflann)

set(SOURCES src/raptor/raptor.cpp
        src/raptor/label_manager.cpp
        src/raptor/state.cpp
        src/transfers/kd_tree.cpp
        src/transfers/linear_walk_calculator.cpp
        src/transfers/transfers.cpp
        src/schedule/gtfs.cpp
        src/schedule/gtfs_calendar.cpp
        src/schedule/gtfs_stop.cpp
        src/schedule/gtfs_stop_time.cpp
        src/schedule/Schedule.cpp
        src/schedule/gtfs.cpp
)

option(PT_ROUTING_BUILD_TESTS "Build unit tests" OFF)
if (PT_ROUTING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

add_library(pt_routing)
target_sources(pt_routing
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include)
target_link_directories(pt_routing PRIVATE ${Boost_INCLUDE_DIRS})
target_sources(pt_routing PRIVATE ${SOURCES})
target_link_libraries(pt_routing PUBLIC just_gtfs nanoflann::nanoflann ${Boost_LIBRARIES})
target_compile_features(pt_routing PUBLIC cxx_std_20)


if (EXISTS "${CMAKE_SOURCE_DIR}/test.cpp")
    add_executable(test_exec test.cpp)
    target_link_libraries(test_exec PRIVATE pt_routing)
endif ()
