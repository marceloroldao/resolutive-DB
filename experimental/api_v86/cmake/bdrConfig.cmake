include(CMakeFindDependencyMacro)
find_dependency(ZLIB REQUIRED)
find_dependency(Threads REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/bdrTargets.cmake")
