cmake_minimum_required(VERSION 3.16)
project(bdr_api_v86 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

# Preserved v0.2/v86 baseline target.
add_library(bdr_v86 STATIC
    src/database.cpp
    src/resolutive_index.cpp
)
target_include_directories(bdr_v86 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bdr_v86 PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v86_contract test_contract.cpp)
target_link_libraries(v86_contract PRIVATE bdr_v86)

# Stable v1 library plus additive v1.1 atomic API. Existing database.hpp and
# bdr::Database remain source-compatible; atomic_database.hpp opts into BDW4.
add_library(bdr_v1_candidate STATIC
    src/database_v1.cpp
    ../api_v111/atomic_database.cpp
    ../api_v110/durable_database.cpp
    ../api_v106/migration.cpp
    ../api_v102/file_wal.cpp
    ../api_v101/atomic_wal.cpp
)
set_target_properties(bdr_v1_candidate PROPERTIES
    EXPORT_NAME bdr
    OUTPUT_NAME bdr
    POSITION_INDEPENDENT_CODE ON
)
target_include_directories(bdr_v1_candidate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_v1_candidate PUBLIC ZLIB::ZLIB Threads::Threads)
add_library(bdr::bdr ALIAS bdr_v1_candidate)

# Stable-language boundary for mobile/native embedding. It deliberately wraps
# the v1.1 AtomicDatabase rather than exposing implementation-specific C++ ABI.
add_library(bdr_c SHARED src/c_api.cpp)
target_include_directories(bdr_c
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_c PRIVATE bdr_v1_candidate)
set_target_properties(bdr_c PROPERTIES OUTPUT_NAME bdr_c)

add_executable(v1_candidate_contract test_contract.cpp)
target_link_libraries(v1_candidate_contract PRIVATE bdr_v1_candidate)

add_executable(v111_public_compat ../api_v111/test_public_compat.cpp)
target_link_libraries(v111_public_compat PRIVATE bdr_v1_candidate)

add_executable(c_api_contract test_c_api.cpp)
target_link_libraries(c_api_contract PRIVATE bdr_c)

# Internal regression fallback: legacy ResolutiveIndex + the same streaming persistence path.
add_library(bdr_v1_fallback STATIC
    src/database_v1.cpp
    src/resolutive_index.cpp
)
target_compile_definitions(bdr_v1_fallback PRIVATE BDR_V1_FALLBACK_INDEX)
target_include_directories(bdr_v1_fallback PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(bdr_v1_fallback PUBLIC ZLIB::ZLIB Threads::Threads)

add_executable(v1_fallback_contract test_contract.cpp)
target_link_libraries(v1_fallback_contract PRIVATE bdr_v1_fallback)

# Cross-version persistence compatibility harness. The same deterministic
# dataset is written/read by three independently linked engine variants.
add_executable(v1_compat_baseline test_v1_cross_compat.cpp)
target_link_libraries(v1_compat_baseline PRIVATE bdr_v86)

add_executable(v1_compat_candidate test_v1_cross_compat.cpp)
target_link_libraries(v1_compat_candidate PRIVATE bdr_v1_candidate)

add_executable(v1_compat_fallback test_v1_cross_compat.cpp)
target_link_libraries(v1_compat_fallback PRIVATE bdr_v1_fallback)

# Install/export the v1 library and the additive C ABI.
install(TARGETS bdr_v1_candidate bdr_c
    EXPORT bdrTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES
    include/bdr/database.hpp
    include/bdr/atomic_database.hpp
    include/bdr/c_api.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bdr
)
install(EXPORT bdrTargets
    FILE bdrTargets.cmake
    NAMESPACE bdr::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)
install(FILES cmake/bdrConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bdr
)

enable_testing()
add_test(NAME v86_contract COMMAND v86_contract)
add_test(NAME v1_candidate_contract COMMAND v1_candidate_contract)
add_test(NAME v1_fallback_contract COMMAND v1_fallback_contract)
add_test(NAME v111_public_compat COMMAND v111_public_compat)
add_test(NAME c_api_contract COMMAND c_api_contract)
