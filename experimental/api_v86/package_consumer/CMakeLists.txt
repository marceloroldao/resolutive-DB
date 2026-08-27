cmake_minimum_required(VERSION 3.16)
project(bdr_v1_package_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(bdr CONFIG REQUIRED)

add_executable(bdr_v1_package_consumer main.cpp)
target_link_libraries(bdr_v1_package_consumer PRIVATE bdr::bdr)
