cmake_minimum_required(VERSION 4.4)
option(USE_LTO "Should we use Link time optimizations" ON)
option(USE_MODULES "Enable modules support in libglox" OFF)
option(USE_COMPILERS_IMPORT_STD "Enable import std support from compiler in libglox" OFF)
option(COMPILE_TESTS "Enable tests?" ON)
option(SET_INSTALL_TARGETS "Should we attach installation targets?" OFF)
option(LIBGLOX_SANITIZE "Use sanitizers" OFF)
set(DEFAULT_ALLOCATOR_PATH "" CACHE STRING
 "Header file path with default allocator that will be included in glox/alloc.hpp
and used as default allocator for containers that require them")


if (USE_COMPILERS_IMPORT_STD)
    set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "f35a9ac6-8463-4d38-8eec-5d6008153e7d")
    set(CMAKE_CXX_MODULE_STD 1)
endif()
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS ON)

project(libglox VERSION 1.0 LANGUAGES CXX)


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

if (USE_MODULES)
    add_library(libglox)
else()
    add_library(libglox INTERFACE)
endif()
add_library(libglox::libglox ALIAS libglox)

include(cmake/get-cpm.cmake)

set_target_properties(libglox PROPERTIES
    PREFIX ""
)

target_include_directories(libglox INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (NOT DEFINED WARNING_FLAGS)
    set(WARNING_FLAGS
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wzero-as-null-pointer-constant>>
    )
endif()

if (NOT DEFINED LIBGLOX_COMPILE_OPTIONS)
    message(TRACE "No additional libglox options specified")
endif()

if (DEFAULT_ALLOCATOR_PATH)
    target_compile_definitions(libglox INTERFACE
        LIBGLOX_DEFAULT_ALLOCATOR_PATH="${DEFAULT_ALLOCATOR_PATH}"
    )
endif()
if (USE_MODULES)
    target_compile_definitions(libglox PUBLIC
        USE_MODULES
    )
    target_sources(libglox
        PUBLIC FILE_SET CXX_MODULES
        FILES modules/glox.cpp
    )
    target_include_directories(libglox PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
else()
    target_compile_definitions(libglox INTERFACE
        GLOX_EXPORT= GLOX_BEGIN_EXPORT= GLOX_END_EXPORT=
    )
endif()
target_compile_options(libglox INTERFACE
    ${WARNING_FLAGS}
    ${LIBGLOX_COMPILE_OPTIONS}
)

# Install config
if (${SET_INSTALL_TARGETS})
    include(GNUInstallDirs)

    install(TARGETS libglox
        LIBRARY DESTINATION lib
    )
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "glox/*.hpp"
    )
    install(DIRECTORY cmake/
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Glox
        FILES_MATCHING PATTERN "*.cmake"
    )
endif()

# Handle options
if (COMPILE_TESTS)
    add_subdirectory(test)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${USE_LTO})
