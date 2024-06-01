cmake_minimum_required(VERSION 3.13)
project(cxx_language_features)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(cxx_language_features ${SRC_FILES} ${HDR_FILES})
