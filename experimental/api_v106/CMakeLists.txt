cmake_minimum_required(VERSION 3.16)
project(bdr_v106_migration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(v106_v1_engine STATIC
    ../api_v86/src/database_v1.cpp
)
target_include_directories(v106_v1_engine PUBLIC ../api_v86/include)
target_link_libraries(v106_v1_engine PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v106_migration
    test_migration.cpp
    migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
target_include_directories(v106_migration PRIVATE ../api_v86/include)
target_link_libraries(v106_migration PRIVATE v106_v1_engine ZLIB::ZLIB Threads::Threads)

enable_testing()
add_test(NAME v106_migration COMMAND v106_migration)
