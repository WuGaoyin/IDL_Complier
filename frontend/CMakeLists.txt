cmake_minimum_required( VERSION 2.8 )

set( CMAKE_CXX_FLAGS "-std=c++17 -lpthread" )

PROJECT(idlc)
MESSAGE("Project: idlc")

option(DEBUG_MODE "option for debug" OFF)
if (DEBUG_MODE) 
	add_definitions(-DDEBUG_MODE)
endif(DEBUG_MODE)

include_directories(
${PROJECT_SOURCE_DIR}/include
)

file(GLOB_RECURSE idlc_srcs ./src/*.cpp ./src/*.cc)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/output/bin)

add_executable(${PROJECT_NAME} ${idlc_srcs})