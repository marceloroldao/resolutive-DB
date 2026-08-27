cmake_minimum_required(VERSION 3.16)
project(bdr_v105_concurrency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_executable(v105_concurrency
    test_concurrency.cpp
    concurrent_database.cpp
    ../api_v104/batch_database.cpp
    ../api_v103/commit_boundary.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)

target_link_libraries(v105_concurrency PRIVATE ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v105_concurrency COMMAND v105_concurrency)
