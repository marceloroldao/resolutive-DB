cmake_minimum_required(VERSION 3.20)
project(bdr_external_consumer LANGUAGES CXX)

find_package(bdr CONFIG REQUIRED)

add_executable(bdr_external_consumer main.cpp)
target_compile_features(bdr_external_consumer PRIVATE cxx_std_20)
target_link_libraries(bdr_external_consumer PRIVATE bdr::bdr)
