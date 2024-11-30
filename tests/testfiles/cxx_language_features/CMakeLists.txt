cmake_minimum_required(VERSION 3.13)
project(testfiles_cxx_language_features)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(testfiles_cxx_language_features ${SRC_FILES} ${HDR_FILES})
