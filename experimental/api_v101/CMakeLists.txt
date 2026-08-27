cmake_minimum_required(VERSION 3.16)
project(bdr_v101 LANGUAGES CXX)

find_package(ZLIB REQUIRED)

add_library(bdr_v101_atomic_wal STATIC
    atomic_wal.cpp
)
target_compile_features(bdr_v101_atomic_wal PUBLIC cxx_std_17)
target_include_directories(bdr_v101_atomic_wal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bdr_v101_atomic_wal PUBLIC ZLIB::ZLIB)

add_executable(v101_atomic_wal_test test_atomic_wal.cpp)
target_link_libraries(v101_atomic_wal_test PRIVATE bdr_v101_atomic_wal)

enable_testing()
add_test(NAME v101_atomic_wal_test COMMAND v101_atomic_wal_test)
