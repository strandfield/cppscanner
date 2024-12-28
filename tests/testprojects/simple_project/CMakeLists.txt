cmake_minimum_required(VERSION 3.13)
project(testproject_simple_project)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(testproject_simple_project "palindrome.h" "palindrome.cpp" "print.h" "print.cpp" "main.cpp")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    find_library(CPPSCANNER_PLUGIN NAMES "libcppscanner-plugin.so")
    if (CPPSCANNER_PLUGIN)
        message("cppscanner's plugin was found: ${CPPSCANNER_PLUGIN}")
        target_compile_options(testproject_simple_project PRIVATE "-fplugin=${CPPSCANNER_PLUGIN}" "-Xclang" "-detailed-preprocessing-record")
    endif()
endif()
