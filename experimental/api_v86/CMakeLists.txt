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

# Internal v1 candidate: CompactIndex + streaming BDR3 checkpoint.
# CompactIndex is header-only, so database_v1.cpp is the only implementation unit.
add_library(bdr_v1_candidate STATIC
    src/database_v1.cpp
)
set_target_properties(bdr_v1_candidate PROPERTIES EXPORT_NAME bdr OUTPUT_NAME bdr)
target_include_directories(bdr_v1_candidate
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(bdr_v1_candidate PUBLIC ZLIB::ZLIB Threads::Threads)
add_library(bdr::bdr ALIAS bdr_v1_candidate)

add_executable(v1_candidate_contract test_contract.cpp)
target_link_libraries(v1_candidate_contract PRIVATE bdr_v1_candidate)

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

# Install/export only the v1 candidate surface. Baseline, fallback and internal
# index headers stay implementation-private; database.hpp is the frozen public API.
install(TARGETS bdr_v1_candidate
    EXPORT bdrTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES include/bdr/database.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bdr)
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
