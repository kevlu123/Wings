cmake_minimum_required(VERSION 3.0)

project(wings)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT DEFINED WINGS_NO_DEV)
	add_subdirectory(wings)
endif()

add_subdirectory(shell)
