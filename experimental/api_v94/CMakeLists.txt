cmake_minimum_required(VERSION 3.16)
project(bdr_api_v94 VERSION 0.0.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

add_library(bdr STATIC
    ../api_v86/src/database.cpp
    ../api_v86/src/resolutive_index.cpp
)
target_include_directories(bdr
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api_v86/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr PUBLIC ZLIB::ZLIB Threads::Threads)
set_target_properties(bdr PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME bdr)
add_library(bdr::bdr ALIAS bdr)

add_library(bdr_c SHARED ../api_v87/src/bdr_c.cpp)
target_include_directories(bdr_c
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../api_v87/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ../api_v86/include
)
target_link_libraries(bdr_c PRIVATE bdr)
set_target_properties(bdr_c PROPERTIES OUTPUT_NAME bdr_c SOVERSION 1 VERSION 1.0.0 EXPORT_NAME bdr_c)
add_library(bdr::c ALIAS bdr_c)

install(TARGETS bdr bdr_c
    EXPORT bdrTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY ../api_v86/include/bdr DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ../api_v87/include/bdr/bdr_c.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bdr)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/bdrConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfigVersion.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(EXPORT bdrTargets
    NAMESPACE bdr::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
