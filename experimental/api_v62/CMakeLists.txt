cmake_minimum_required(VERSION 3.20)
project(bdr_python_bridge_v62 VERSION 0.62.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

set(V56 ${CMAKE_CURRENT_SOURCE_DIR}/../api_v56)

add_library(bdr_core_v62 STATIC
    ${V56}/src/database.cpp
    ${V56}/src/resolutive_index.cpp
)
target_compile_features(bdr_core_v62 PUBLIC cxx_std_20)
target_include_directories(bdr_core_v62 PUBLIC ${V56}/include)
target_link_libraries(bdr_core_v62 PUBLIC Threads::Threads ZLIB::ZLIB)
set_target_properties(bdr_core_v62 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(bdr_c SHARED ${V56}/src/c_api.cpp)
target_include_directories(bdr_c PUBLIC ${V56}/include)
target_link_libraries(bdr_c PRIVATE bdr_core_v62)
set_target_properties(bdr_c PROPERTIES OUTPUT_NAME bdr_c)

install(TARGETS bdr_c LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
