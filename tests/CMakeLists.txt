
enable_testing()

# test files

set (PROJECT_LISTS_STR "")
set (PROJECT_LISTS_STR "${PROJECT_LISTS_STR}\nstatic constexpr const char* TESTFILES_DIRECTORY = \"${CMAKE_CURRENT_LIST_DIR}/testfiles\";")

add_subdirectory(testfiles)

# tests with example projects

macro(setup_test_project folderName)
  add_subdirectory(${folderName})
  set(WDIR "${CMAKE_BINARY_DIR}/${folderName}/build")
  
  if (NOT EXISTS ${WDIR})
    file(MAKE_DIRECTORY ${WDIR})
  endif()
  
  if (WIN32)
    execute_process(
      COMMAND ${CMAKE_COMMAND} "-G" "NMake Makefiles" "${PROJECT_SOURCE_DIR}/tests/${folderName}" 
      WORKING_DIRECTORY ${WDIR}
    )
  else()
    execute_process(
      COMMAND ${CMAKE_COMMAND} "${PROJECT_SOURCE_DIR}/tests/${folderName}" 
      WORKING_DIRECTORY ${WDIR}
    )
  endif()
  
  string(TOUPPER ${folderName} FOLDER)
  set (${FOLDER}_BUILD_DIR ${WDIR})
  set (PROJECT_LISTS_STR "${PROJECT_LISTS_STR}\nstatic constexpr const char* ${FOLDER}_ROOT_DIR = \"${CMAKE_CURRENT_LIST_DIR}/${folderName}\";")
  set (PROJECT_LISTS_STR "${PROJECT_LISTS_STR}\nstatic constexpr const char* ${FOLDER}_BUILD_DIR = \"${WDIR}\";")
endmacro()

setup_test_project(simple_project)
setup_test_project(hello_world)
setup_test_project(cxx_language_features)

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/catch.hpp")
  message("Downloading Catch2")
  file(DOWNLOAD https://github.com/catchorg/Catch2/releases/download/v2.13.10/catch.hpp "${CMAKE_CURRENT_BINARY_DIR}/catch.hpp" SHOW_PROGRESS)
endif()

configure_file("projects.h.in" "projects.h")

add_executable(TEST_cppscanner "${CMAKE_CURRENT_BINARY_DIR}/catch.hpp" "${CMAKE_CURRENT_BINARY_DIR}/projects.h" "main.cpp" "helpers.h" "projects.h.in"
  "simple_project.cpp"
  "hello_world.cpp"
  "cxx_language_features.cpp"
  "cpp20_language_features.cpp"
  "stl.cpp"
)
target_link_libraries(TEST_cppscanner scannerInvocation)
target_include_directories(TEST_cppscanner PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (WIN32)
  set_target_properties(TEST_cppscanner PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${SQLite3_INCLUDE_DIR}/../bin;%PATH%")
endif()

# self test

set(SELFTEST_BUILD_DIR "${CMAKE_BINARY_DIR}/self_test")
if (NOT EXISTS ${SELFTEST_BUILD_DIR})
  file(MAKE_DIRECTORY ${SELFTEST_BUILD_DIR})
endif()

if (WIN32)
  execute_process(
    COMMAND ${CMAKE_COMMAND} "-G" "NMake Makefiles" "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" "-DLLVM_DIR=${LLVM_DIR}" "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" "-DCMAKE_BUILD_TYPE=Debug" "-DBUILD_TESTS=OFF" "${PROJECT_SOURCE_DIR}" 
    WORKING_DIRECTORY ${SELFTEST_BUILD_DIR}
  )
else()
  execute_process(
    COMMAND ${CMAKE_COMMAND} "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" "-DLLVM_DIR=${LLVM_DIR}" "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" "-DCMAKE_BUILD_TYPE=Debug" "-DBUILD_TESTS=OFF" "${PROJECT_SOURCE_DIR}" 
    WORKING_DIRECTORY ${SELFTEST_BUILD_DIR}
  )
endif()

add_executable(TEST_cppscanner_selftest "${CMAKE_CURRENT_BINARY_DIR}/catch.hpp" "selftest.cpp" "helpers.h")
target_compile_definitions(TEST_cppscanner_selftest PUBLIC SELFTEST_BUILD_DIR="${SELFTEST_BUILD_DIR}")
target_compile_definitions(TEST_cppscanner_selftest PUBLIC PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
target_link_libraries(TEST_cppscanner_selftest scannerInvocation)
target_include_directories(TEST_cppscanner_selftest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (WIN32)
  set_target_properties(TEST_cppscanner_selftest PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${SQLite3_INCLUDE_DIR}/../bin;%PATH%")
endif()

## cmake tests

add_test(NAME TEST_cppscanner COMMAND TEST_cppscanner)
add_test(NAME TEST_cppscanner_selftest COMMAND TEST_cppscanner_selftest)

if (WIN32)
  set_tests_properties(TEST_cppscanner TEST_cppscanner_selftest
                     PROPERTIES ENVIRONMENT "PATH=${SQLite3_INCLUDE_DIR}/../bin;%PATH%")
endif()