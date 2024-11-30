cmake_minimum_required(VERSION 3.13)
project(testfiles_stl)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES "*.cpp")

add_library(testfiles_stl STATIC ${SRC_FILES})