cmake_minimum_required(VERSION 3.31)
project(pt_routing)

set(CMAKE_CXX_STANDARD 20)

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

set(SOURCES src/raptor/raptor.cpp src/schedule/Schedule.cpp
        src/schedule/gtfs.cpp
        src/raptor/StopKDTree.cpp)

option(PT_ROUTING_BUILD_TESTS "Build unit tests" ON)
include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)
if(PT_ROUTING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

add_library(pt_routing_base INTERFACE ${SOURCES})
target_include_directories(pt_routing_base INTERFACE include)
target_link_libraries(pt_routing_base INTERFACE just_gtfs nanoflann::nanoflann)

add_executable(pt_routing main.cpp)
target_link_libraries(pt_routing PRIVATE pt_routing_base)
target_compile_features(pt_routing PUBLIC cxx_std_20)

if (EXISTS "test.cpp")
    add_executable(test test.cpp ${SOURCES})
    target_link_libraries(test PRIVATE pt_routing_base)
endif ()