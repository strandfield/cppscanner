cmake_minimum_required(VERSION 3.13)
project(testfiles_cpp20_library_features)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(testfiles_cpp20_library_features ${HDR_FILES} ${SRC_FILES})
