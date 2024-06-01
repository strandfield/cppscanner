cmake_minimum_required(VERSION 3.13)
project(stl)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB SRC_FILES "*.cpp")

add_library(stl STATIC ${SRC_FILES})