cmake_minimum_required(VERSION 3.20)
project(bdr VERSION 0.60.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_library(bdr
    src/database.cpp
    src/resolutive_index.cpp
)
add_library(bdr::bdr ALIAS bdr)

target_compile_features(bdr PUBLIC cxx_std_20)
target_include_directories(bdr
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr PUBLIC Threads::Threads ZLIB::ZLIB)
set_target_properties(bdr PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
    EXPORT_NAME bdr
)

option(BDR_BUILD_TESTS "Build experimental BDR API tests" ON)
if(BDR_BUILD_TESTS)
    enable_testing()
    add_executable(bdr_api_v56_test tests/test_api.cpp)
    target_link_libraries(bdr_api_v56_test PRIVATE bdr::bdr)
    add_test(NAME bdr_api_contract COMMAND bdr_api_v56_test)
endif()

install(TARGETS bdr
    EXPORT bdrTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT bdrTargets
    FILE bdrTargets.cmake
    NAMESPACE bdr::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bdrConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/bdrConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
