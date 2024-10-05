cmake_minimum_required(VERSION 3.13)
project(cppscanner VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

set(CPPSCANNER_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR})

SET(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG HINTS "${LLVM_INSTALL_PREFIX}/lib/cmake/clang")

add_subdirectory(modules)
add_subdirectory(src)
add_subdirectory(tools)

if (NOT DEFINED BUILD_TESTS)
  set(BUILD_TESTS ON CACHE BOOL "whether building tests is enabled")
endif()

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
