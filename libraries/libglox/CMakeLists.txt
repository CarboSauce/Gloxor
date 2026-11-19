cmake_minimum_required(VERSION 4.5)
option(USE_LTO "Should we use Link time optimizations" ON)
option(USE_MODULES "Enable modules support in libglox" OFF)
option(COMPILE_TESTS "Enable tests?" ON)
option(SET_INSTALL_TARGETS "Should we attach installation targets?" OFF)
option(LIBGLOX_SANITIZE "Use sanitizers" OFF)
option(USE_CUSTOM_DEFAULT_ALLOCATOR "Use custom default allocator routines" OFF)
set(DEFAULT_ALLOCATOR_PATH "" CACHE STRING
 "Header file path with default allocator that will be included in glox/alloc.hpp
and used as default allocator for containers that require them")


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

if (PROJECT_IS_TOP_LEVEL AND NOT DEFINED WARNING_FLAGS)
    set(WARNING_FLAGS
        $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wzero-as-null-pointer-constant>>
    )
endif()

if (NOT DEFINED LIBGLOX_COMPILE_OPTIONS)
    message(TRACE "No additional libglox options specified")
endif()

if (USE_CUSTOM_DEFAULT_ALLOCATOR)
    if (USE_MODULES)
        target_compile_definitions(libglox PUBLIC
            CUSTOM_DEFAULT_ALLOCATOR
        )
    else()
        target_compile_definitions(libglox INTERFACE
            CUSTOM_DEFAULT_ALLOCATOR
        )
    endif()
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
    target_compile_features(libglox
        PUBLIC cxx_std_26
    )
else()
    target_compile_definitions(libglox INTERFACE
        GLOX_EXPORT= GLOX_BEGIN_EXPORT= GLOX_END_EXPORT=
    )
endif()

if (USE_MODULES)
    target_compile_options(libglox PRIVATE
        ${WARNING_FLAGS}
        ${LIBGLOX_COMPILE_OPTIONS}
    )
else()
    target_compile_options(libglox INTERFACE
        ${WARNING_FLAGS}
        ${LIBGLOX_COMPILE_OPTIONS}
    )
endif()

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
    enable_testing()
    add_subdirectory(test)
endif()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${USE_LTO})
