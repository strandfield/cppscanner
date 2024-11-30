cmake_minimum_required(VERSION 3.13)
project(testproject_hello_world)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(testproject_hello_world "hello.h" "main.cpp")