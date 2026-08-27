cmake_minimum_required(VERSION 3.16)
project(bdr_v107_integrated LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(v107_v1_engine STATIC
    ../api_v86/src/database_v1.cpp
)
target_include_directories(v107_v1_engine PUBLIC ../api_v86/include)
target_link_libraries(v107_v1_engine PUBLIC ZLIB::ZLIB Threads::Threads)

add_library(v107_candidate STATIC
    integrated_database.cpp
    ../api_v106/migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v107_candidate PUBLIC . ../api_v86/include)
target_link_libraries(v107_candidate PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v107_integrated test_integrated.cpp)
target_link_libraries(v107_integrated PRIVATE v107_candidate v107_v1_engine ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v107_integrated COMMAND v107_integrated)
