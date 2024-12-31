
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(genjs ${SRC_FILES})
target_link_libraries(genjs index)

set_target_properties(genjs PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(genjs PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (WIN32)
  set_target_properties(genjs PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${SQLite3_INCLUDE_DIR}/../bin;%PATH%")
endif()
