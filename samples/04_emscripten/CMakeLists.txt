cmake_minimum_required(VERSION 3.8)
#cmake_policy(SET CMP0015 NEW)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXECUTABLE_SUFFIX ".html")
#set(COMPILE_DEFINITIONS _PLATFORM_WASM)
set(LINK_OPTIONS ${LINK_OPTIONS} "-sUSE_GLFW=3 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sNO_EXIT_RUNTIME=0 -sASSERTIONS=1")
set(LINK_LIBRARIES ${LINK_LIBRARIES} -lglfw3 -lGLESv2 -lEGL)

project(sample_emscripten)

include("../../imapp.cmake")

#set(CMAKE_VERBOSE_MAKEFILE on)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build.emscripten)

#include_directories("lib/assimp/include")

#link_directories("lib/assimp/lib-js")
#link_libraries("assimp")

#file(GLOB_RECURSE CORE_HDR src/*.h)
#file(GLOB_RECURSE CORE_SRC src/*.cpp)

#
# Trying usual cmake project compiler execution
#

#add_definitions("-std=c++11")

add_executable(${CMAKE_PROJECT_NAME}
			   ${ImAppSrcFiles}
			   main.c)

#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Os -s SIDE_MODULE=1 ")
#set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS "-Os -s WASM=1 -s SIDE_MODULE=1 -s STANDALONE_WASM --no-entry")

#set_target_properties(client PROPERTIES LINK_FLAGS "-s DEMANGLE_SUPPORT=1 --preload-file ${CMAKE_SOURCE_DIR}/assets --bind")