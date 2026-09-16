cmake_minimum_required(VERSION 3.16)
project(resolutive_db VERSION 1.2.0 LANGUAGES CXX)

# Repository entry point for the v1.2.0-rc1 candidate line.
# v1.1.0 remains the current published stable baseline; v1.2.0-rc1 adds
# the Python atomic bridge and validated bulk-read path without redesigning BDW4.
add_subdirectory(experimental/api_v86)