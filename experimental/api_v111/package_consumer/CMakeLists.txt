cmake_minimum_required(VERSION 3.16)
project(bdr_v111_external_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(bdr CONFIG REQUIRED)
add_executable(v111_external_consumer main.cpp)
target_link_libraries(v111_external_consumer PRIVATE bdr::bdr)
