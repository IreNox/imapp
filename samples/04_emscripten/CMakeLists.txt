cmake_minimum_required(VERSION 3.22.1)

project(sample_emscripten)

include("../../imapp.cmake")

#set(CMAKE_VERBOSE_MAKEFILE on)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build.emscripten)

add_executable(${CMAKE_PROJECT_NAME}
			   ${ImAppCFiles}
			   main.c)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

#find_package(GLEW REQUIRED)
#find_package(glfw3 REQUIRED)

set(COMPILE_OPTIONS "--use-port=sdl2")
set(LINK_OPTIONS "-g -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sUSE_GLFW=3 -sWASM=1 -sALLOW_MEMORY_GROWTH=1")

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_compile_definitions(DEBUG)
	set(COMPILE_OPTIONS "${COMPILE_OPTIONS} -O0 -g")
	set(LINK_OPTIONS "${LINK_OPTIONS} -g -sASSERTIONS=1")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
	set(LINK_OPTIONS "${LINK_OPTIONS} -O3 -sASSERTIONS=0")
endif()

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES COMPILE_FLAGS ${COMPILE_OPTIONS})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_FLAGS ${LINK_OPTIONS})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINK_LIBRARIES "-lglfw3 -lGLESv2 -lEGL -lSDL2")

#set_target_properties(client PROPERTIES LINK_FLAGS "-s DEMANGLE_SUPPORT=1 --preload-file ${CMAKE_SOURCE_DIR}/assets --bind")

#message("CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
#message("CMAKE_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}")
