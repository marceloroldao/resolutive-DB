cmake_minimum_required(VERSION 3.16)
project(bdr_v104 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()
find_package(ZLIB REQUIRED)

add_executable(test_v104_batch_database
    test_batch_database.cpp
    batch_database.cpp
    ../api_v103/commit_boundary.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)

target_include_directories(test_v104_batch_database PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../api_v103
    ${CMAKE_CURRENT_SOURCE_DIR}/../api_v102
    ${CMAKE_CURRENT_SOURCE_DIR}/../api_v101
)

target_link_libraries(test_v104_batch_database PRIVATE ZLIB::ZLIB)
add_test(NAME v104_batch_database COMMAND test_v104_batch_database)
