

macro(cppscanner_source_group files)
  foreach(_source IN ITEMS ${files})
    get_filename_component(_source_path "${_source}" PATH)
    file(RELATIVE_PATH _source_path_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source_path}")
    string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
    source_group("${_group_path}" FILES "${_source}")
  endforeach()
endmacro()

macro(add_module folder)
  file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cppscanner/${folder}/*.cpp")
  file(GLOB_RECURSE HDR_FILES "${CMAKE_CURRENT_SOURCE_DIR}/cppscanner/${folder}/*.h")
  add_library(${folder} STATIC ${SRC_FILES} ${HDR_FILES})
  target_include_directories(${folder} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  set(files ${SRC_FILES} ${HDR_FILES})
  cppscanner_source_group("${files}")
endmacro()

add_module(base)

add_module(index)

find_package(SQLite3)
add_module(database)
target_link_libraries(database SQLite::SQLite3)

add_module(snapshot)
target_link_libraries(snapshot base index database)

add_module(indexer)
target_link_libraries(indexer snapshot)
target_link_libraries(indexer 
  clangFrontend
  clangParse
  clangSema
  clangAST
  clangBasic
  clangLex
  clangTooling
  clangIndex
  clangDriver
)
target_include_directories(indexer SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})
if (WIN32 AND NOT SELF_PARSE)
  # https://github.com/llvm/llvm-project/issues/86028
  target_compile_options(indexer PRIVATE "/Zc:preprocessor")
endif()

add_module(cmakeIntegration)
target_link_libraries(cmakeIntegration 
  LLVMSupport
)
target_include_directories(cmakeIntegration SYSTEM PUBLIC ${LLVM_INCLUDE_DIRS})


add_module(scannerInvocation)
target_link_libraries(scannerInvocation indexer cmakeIntegration)
