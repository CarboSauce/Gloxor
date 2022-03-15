cmake_minimum_required(VERSION 3.14)
###########
# all ugly variables that had to be set
# thanks to cmake not supporting osdev that much
# feel free to add other hack variables 
###########
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_SYSTEM_NAME "Generic")
###
project(Gloxor VERSION 1.0 LANGUAGES C CXX ASM_NASM)
set(CMAKE_VERBOSE_MAKEFILE TRUE)

include(CMakeOptions.cmake)
include(CheckCXXSourceCompiles)


if (FORCE_DEBUG)
    add_compile_definitions(DEBUGBUILD)
endif()
if (COMPILE_TESTS)
    add_compile_definitions(GLOXTESTING)
endif()


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Detected Clang")
    include(cmake/ClangToolchain.cmake)
endif()



set(WARNING_FLAGS
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wno-interrupt-service-routine>>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wno-interrupt-service-routine>>
)
set(GLOX_COMPILE_FLAGS 
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:${CMMN_FLAGS}>>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:${CMMN_FLAGS}>>
)

if (USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${USE_LTO})
endif()


add_subdirectory(libraries)
add_subdirectory(kernel)
