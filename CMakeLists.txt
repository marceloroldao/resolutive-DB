cmake_minimum_required(VERSION 3.16)
project(resolutive_db VERSION 1.1.0 LANGUAGES CXX)

# Repository entry point for the v1.1 stable-engine line.
# v1.0.0 remains the immutable published baseline; v1.1.0 adds the
# opt-in atomic BDW4 write path while preserving the v1.0 source contract.
add_subdirectory(experimental/api_v86)
