cmake_minimum_required(VERSION 2.8)
project(dyad C)

set (DYAD_VERSION_MAJOR 0)
set (DYAD_VERSION_MINOR 2)
set (DYAD_VERSION_PATCH 1)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
endif(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

add_subdirectory(src)
