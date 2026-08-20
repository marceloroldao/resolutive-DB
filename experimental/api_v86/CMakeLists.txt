cmake_minimum_required(VERSION 3.16)
project(bdr_api_v86 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(bdr_v86 STATIC
    src/database.cpp
    src/resolutive_index.cpp
)
target_include_directories(bdr_v86 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bdr_v86 PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v86_contract test_contract.cpp)
target_link_libraries(v86_contract PRIVATE bdr_v86)

enable_testing()
add_test(NAME v86_contract COMMAND v86_contract)
