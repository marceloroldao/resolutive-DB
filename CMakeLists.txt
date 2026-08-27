cmake_minimum_required(VERSION 3.16)
project(resolutive_db VERSION 1.1.0 LANGUAGES CXX)

# Repository entry point for the v1 line.
# v1.0.0 remains the immutable published stable baseline; develop/v1.1.0
# carries the additive atomic-write candidate validated against that baseline.
add_subdirectory(experimental/api_v86)
