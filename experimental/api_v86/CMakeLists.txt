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
set_target_properties(bdr_v1_candidate PROPERTIES EXPORT_NAME bdr OUTPUT_NAME bdr)
target_include_directories(bdr_v1_candidate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_v1_candidate PUBLIC ZLIB::ZLIB Threads::Threads)
add_library(bdr::bdr ALIAS bdr_v1_candidate)

# Stable basic C boundary over bdr::Database.
add_library(bdr_c_api STATIC src/c_api.cpp)
set_target_properties(bdr_c_api PROPERTIES EXPORT_NAME c_api OUTPUT_NAME bdr_c_api)
target_include_directories(bdr_c_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_c_api PUBLIC bdr_v1_candidate)
add_library(bdr::c_api ALIAS bdr_c_api)

# Additive atomic mobile boundary. One logical Memoria.ia memory/turn must be
# committed through this target as one AtomicDatabase/BDW4 batch.
add_library(bdr_atomic_c_api STATIC src/atomic_c_api.cpp)
set_target_properties(bdr_atomic_c_api PROPERTIES EXPORT_NAME atomic_c_api OUTPUT_NAME bdr_atomic_c_api)
target_include_directories(bdr_atomic_c_api
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_atomic_c_api PUBLIC bdr_v1_candidate)
add_library(bdr::atomic_c_api ALIAS bdr_atomic_c_api)

add_executable(v1_candidate_contract test_contract.cpp)
target_link_libraries(v1_candidate_contract PRIVATE bdr_v1_candidate)

add_executable(v111_public_compat ../api_v111/test_public_compat.cpp)
target_link_libraries(v111_public_compat PRIVATE bdr_v1_candidate)

add_executable(v1_c_api_restart test_c_api_restart.cpp)
target_link_libraries(v1_c_api_restart PRIVATE bdr_c_api)

add_executable(v1_atomic_c_api_restart test_atomic_c_api_restart.cpp)
target_link_libraries(v1_atomic_c_api_restart PRIVATE bdr_atomic_c_api)

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

install(TARGETS bdr_v1_candidate bdr_c_api bdr_atomic_c_api
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
    include/bdr/atomic_c_api.h
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
add_test(NAME v1_c_api_restart COMMAND v1_c_api_restart)
add_test(NAME v1_atomic_c_api_restart COMMAND v1_atomic_c_api_restart)
