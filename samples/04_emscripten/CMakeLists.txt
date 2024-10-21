cmake_minimum_required(VERSION 3.22.1)
#cmake_policy(SET CMP0015 NEW)

#set(CMAKE_C_STANDARD 11)
#see end: set(CMAKE_EXECUTABLE_SUFFIX ".html")
#set(COMPILE_DEFINITIONS _PLATFORM_WASM)
#set(EMCC)
#set(EMCC_LINKER_OPTIONS "${LINK_OPTIONS} -sMIN_WEBGL_VERSION=2 -sUSE_GLFW=3 -sWASM=1 -sALLOW_MEMORY_GROWTH=1")
#set(CMAKE_CXX_LINK_LIBRARIES "${LINK_LIBRARIES} -lglfw3 -lGLESv2 -lEGL")



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

set(CMAKE_EXECUTABLE_SUFFIX ".html")

#find_package(GLEW REQUIRED)
#find_package(glfw3 REQUIRED)

set(COMPILE_OPTIONS "--use-port=sdl2")
set(LINK_OPTIONS "-g -sMIN_WEBGL_VERSION=2 -sUSE_GLFW=3 -sWASM=1 -sALLOW_MEMORY_GROWTH=1")

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(DEBUG)
	set(COMPILE_OPTIONS "${COMPILE_OPTIONS} -g")
	set(LINK_OPTIONS "${LINK_OPTIONS} -g -sASSERTIONS=1")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(LINK_OPTIONS "${LINK_OPTIONS} -sASSERTIONS=0")
endif()

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES COMPILE_FLAGS ${COMPILE_OPTIONS})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS ${LINK_OPTIONS})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_LIBRARIES "-lglfw3 -lGLESv2 -lEGL -lSDL2")

#set_target_properties(client PROPERTIES LINK_FLAGS "-s DEMANGLE_SUPPORT=1 --preload-file ${CMAKE_SOURCE_DIR}/assets --bind")

#message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
#message("CMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}")
