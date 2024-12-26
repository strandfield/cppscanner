
file(GLOB_RECURSE SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HDR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/**.h)

add_library(cppscanner-plugin MODULE ${SRC_FILES} ${HDR_FILES})
target_link_libraries(cppscanner-plugin indexer)

set_target_properties(cppscanner-plugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set_target_properties(cppscanner-plugin PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
