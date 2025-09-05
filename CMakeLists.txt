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

set(NANOFLANN_BUILD_TESTS OFF)
FetchContent_Declare(
        nanoflann
        GIT_REPOSITORY https://github.com/jlblancoc/nanoflann
        GIT_TAG v1.7.1
        EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(just_gtfs nanoflann)

set(SOURCES src/raptor/raptor.cpp src/raptor/schedule/Schedule.cpp
        src/raptor/schedule/gtfs.cpp)

add_executable(pt_routing main.cpp ${SOURCES})
target_link_libraries(pt_routing PRIVATE just_gtfs nanoflann::nanoflann)
target_include_directories(pt_routing PRIVATE include)
target_compile_features(pt_routing PUBLIC cxx_std_20)

add_executable(test test.cpp ${SOURCES})
target_link_libraries(test PRIVATE just_gtfs ${Boost_LIBRARIES} nanoflann::nanoflann)
target_include_directories(test PRIVATE include ${Boost_INCLUDE_DIRS})
