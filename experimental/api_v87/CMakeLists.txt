cmake_minimum_required(VERSION 3.16)
project(bdr_api_v87 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(bdr_v86 STATIC
    ../api_v86/src/database.cpp
    ../api_v86/src/resolutive_index.cpp
)
target_include_directories(bdr_v86 PUBLIC ../api_v86/include)
target_link_libraries(bdr_v86 PUBLIC ZLIB::ZLIB Threads::Threads)
set_target_properties(bdr_v86 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(bdr_c_v1 SHARED src/bdr_c.cpp)
target_include_directories(bdr_c_v1 PUBLIC include ../api_v86/include)
target_link_libraries(bdr_c_v1 PRIVATE bdr_v86)
set_target_properties(bdr_c_v1 PROPERTIES OUTPUT_NAME bdr_c SOVERSION 1 VERSION 1.0.0)

add_executable(v87_c_abi test_c_abi.c)
target_include_directories(v87_c_abi PRIVATE include)
target_link_libraries(v87_c_abi PRIVATE bdr_c_v1)

enable_testing()
add_test(NAME v87_c_abi COMMAND v87_c_abi)
