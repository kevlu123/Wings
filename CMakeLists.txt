cmake_minimum_required(VERSION 3.0)

project(wings)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(WINGS_BUILD_DEV "Build the development project" OFF)
option(WINGS_BUILD_SHELL "Build the interactive shell" OFF)
option(WINGS_BUILD_STATIC_LIB "Build a static library" OFF)
option(WINGS_BUILD_SHARED_LIB "Build a shared library" OFF)

if (WINGS_BUILD_DEV)
	add_subdirectory(dev)
endif()

if (WINGS_BUILD_SHELL)
	add_subdirectory(shell)
endif()

if (WINGS_BUILD_STATIC_LIB)
	add_subdirectory(static_lib)
endif()

if (WINGS_BUILD_SHARED_LIB)
	add_subdirectory(shared_lib)
endif()
