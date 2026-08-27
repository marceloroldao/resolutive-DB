cmake_minimum_required(VERSION 3.16)
project(resolutive_db VERSION 1.0.0 LANGUAGES CXX)

# Stable repository entry point for the v1 engine.
# The validated implementation remains physically located under experimental/api_v86
# to avoid moving byte-validated source files during finalization.
add_subdirectory(experimental/api_v86)
