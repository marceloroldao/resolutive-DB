cmake_minimum_required(VERSION 3.16)
project(bdr_v102 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)

enable_testing()

add_executable(v102_file_wal
    test_file_wal.cpp
    file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v102_file_wal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../api_v101
)
target_link_libraries(v102_file_wal PRIVATE ZLIB::ZLIB)

add_test(NAME v102_file_wal COMMAND v102_file_wal)
