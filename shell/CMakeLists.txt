cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 20)

add_executable(wings main.cpp)

target_include_directories(wings PRIVATE "${PROJECT_SOURCE_DIR}/single_include")
