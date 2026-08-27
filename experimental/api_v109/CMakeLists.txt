cmake_minimum_required(VERSION 3.16)
project(bdr_v109_integrated_crash LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(v109_v1_engine STATIC
    ../api_v86/src/database_v1.cpp
)
target_include_directories(v109_v1_engine PUBLIC ../api_v86/include)
target_link_libraries(v109_v1_engine PUBLIC ZLIB::ZLIB Threads::Threads)

add_library(v109_candidate STATIC
    ../api_v107/integrated_database.cpp
    ../api_v106/migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v109_candidate PUBLIC
    ../api_v107
    ../api_v106
    ../api_v102
    ../api_v101
    ../api_v86/include
)
target_link_libraries(v109_candidate PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v109_integrated_crash test_integrated_crash.cpp)
target_include_directories(v109_integrated_crash PRIVATE ../api_v86/include)
target_link_libraries(v109_integrated_crash PRIVATE v109_candidate v109_v1_engine ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v109_integrated_crash COMMAND v109_integrated_crash)
