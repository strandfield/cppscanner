cmake_minimum_required(VERSION 3.13)
project(testproject_precompiled_headers)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(testproject_precompiled_headers "main.cpp")
target_precompile_headers(testproject_precompiled_headers PUBLIC precompile.h)
