cmake_minimum_required(VERSION 3.16)
project(bdr_v110_durability LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(v110_database STATIC
    durable_database.cpp
    ../api_v106/migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v110_database PUBLIC
    .
    ../api_v106
    ../api_v102
    ../api_v101
)
target_link_libraries(v110_database PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v110_durability test_durability.cpp)
target_link_libraries(v110_durability PRIVATE v110_database ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v110_durability COMMAND v110_durability)
