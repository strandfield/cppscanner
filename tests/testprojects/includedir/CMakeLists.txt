cmake_minimum_required(VERSION 3.13)
project(testproject_includedir)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(testproject_includedir "main.cpp")
target_include_directories(testproject_includedir PUBLIC "include")
