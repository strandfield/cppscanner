
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HDR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/**.h)

add_executable(cppscanner ${SRC_FILES} ${HDR_FILES})
target_link_libraries(cppscanner scannerInvocation)

set_target_properties(cppscanner PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(cppscanner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if (WIN32)
  set_target_properties(cppscanner PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${SQLite3_INCLUDE_DIR}/../bin;%PATH%")
endif()
