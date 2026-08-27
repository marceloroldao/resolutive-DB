cmake_minimum_required(VERSION 3.16)
project(bdr_v108_stress LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(v108_candidate STATIC
    ../api_v107/candidate_database.cpp
    ../api_v106/migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v108_candidate PUBLIC
    ../api_v107
    ../api_v106
    ../api_v102
    ../api_v101
    ../api_v86/include
)
target_link_libraries(v108_candidate PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v108_stress test_stress.cpp)
target_link_libraries(v108_stress PRIVATE v108_candidate ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v108_stress COMMAND v108_stress)
