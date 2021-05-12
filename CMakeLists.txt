cmake_minimum_required(VERSION 3.14)
###
#   CMake variables that have to be set so that 
#   initial tests dont assume host platform
###

include(CmakeHax.txt)

project(Gloxor VERSION 1.0 LANGUAGES C CXX ASM_NASM)

include(CMakeOptions.txt)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

set_property(GLOBAL PROPERTY 
             CXX_STANDARD 17
             C_STANDARD 11
)

enable_language(ASM_NASM)

add_subdirectory(kernel)
