cmake_minimum_required(VERSION 3.20)
project(bdr_native_wheel VERSION 0.76.0 LANGUAGES C CXX)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

set(V56 ${CMAKE_CURRENT_SOURCE_DIR}/../api_v56)

add_library(bdr_core_v76 STATIC
    ${V56}/src/database.cpp
    ${V56}/src/resolutive_index.cpp
)
target_compile_features(bdr_core_v76 PUBLIC cxx_std_20)
target_include_directories(bdr_core_v76 PUBLIC ${V56}/include)
target_link_libraries(bdr_core_v76 PUBLIC Threads::Threads ZLIB::ZLIB)
set_target_properties(bdr_core_v76 PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(bdr_c SHARED ${V56}/src/c_api.cpp)
target_compile_features(bdr_c PRIVATE cxx_std_20)
target_include_directories(bdr_c PRIVATE ${V56}/include)
target_link_libraries(bdr_c PRIVATE bdr_core_v76)
set_target_properties(bdr_c PROPERTIES OUTPUT_NAME bdr_c)

# The native library is installed inside the Python package so ctypes can
# resolve it without BDR_C_LIBRARY or a system-wide installation.
install(TARGETS bdr_c
    LIBRARY DESTINATION bdr_native
    RUNTIME DESTINATION bdr_native
)
