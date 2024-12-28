cmake_minimum_required(VERSION 3.13)
project(cppscanner VERSION 0.1.0)

# TODO: harmozine option names
option(CPPSCANNER_BUILD_PLUGIN "Build the clang plugin")
option(BUILD_TESTS "Build the tests")

set(CMAKE_CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

set(CPPSCANNER_PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR})

if (UNIX)
  add_compile_options(-fno-rtti)
endif()

SET(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG HINTS "${LLVM_INSTALL_PREFIX}/lib/cmake/clang")

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CPPSCANNER_SNAPSHOT_BUILD)
    find_library(CPPSCANNER_PLUGIN NAMES "libcppscanner-plugin.so")
    if (CPPSCANNER_PLUGIN)
        message("cppscanner's plugin was found: ${CPPSCANNER_PLUGIN}")
        add_compile_options("-fplugin=${CPPSCANNER_PLUGIN}" "-Xclang" "-detailed-preprocessing-record")
    endif()
endif()

add_subdirectory(modules)
add_subdirectory(apps)

if (CPPSCANNER_BUILD_PLUGIN)
  add_subdirectory(plugin)
endif()

if (BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
