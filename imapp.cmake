cmake_minimum_required(VERSION 3.22.1)

include_directories("${CMAKE_CURRENT_LIST_DIR}/include"
					"${CMAKE_CURRENT_LIST_DIR}/submodules/imui/include"
					"${CMAKE_CURRENT_LIST_DIR}/submodules/libspng"
					"${CMAKE_CURRENT_LIST_DIR}/submodules/miniz")
					
add_compile_definitions("SPNG_USE_MINIZ")
add_compile_definitions("SPNG_STATIC")

file(GLOB ImAppCFiles	"${CMAKE_CURRENT_LIST_DIR}/include/imapp/*.h"
						"${CMAKE_CURRENT_LIST_DIR}/src/*.h"
						"${CMAKE_CURRENT_LIST_DIR}/src/*.c"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/imui/src/*.h"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/imui/src/*.c"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/libspng/spng/*.h"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/libspng/spng/*.c"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/miniz/*.h"
						"${CMAKE_CURRENT_LIST_DIR}/submodules/miniz/*.c")

file(GLOB ImAppCppFiles "${CMAKE_CURRENT_LIST_DIR}/submodules/imui/src/*.cpp")
