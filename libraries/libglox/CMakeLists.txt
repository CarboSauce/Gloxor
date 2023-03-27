cmake_minimum_required(VERSION 3.18)
project(libglox VERSION 1.0 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )

option(USE_LTO "Should we use Link time optimizations" ON)
option(COMPILE_TESTS "Enable tests?" ON)
option(SET_INSTALL_TARGETS "Should we attach installation targets?" OFF)
option(LIBGLOX_SANITIZE "Use sanitizers" OFF)
set(DEFAULT_ALLOCATOR_PATH "" CACHE STRING 
	"Header file path with default allocator that will be included in glox/alloc.hpp
and used as default allocator for containers that require them")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(libglox INTERFACE 
)

set_target_properties(libglox PROPERTIES
	PREFIX ""
)

target_include_directories(libglox INTERFACE 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (NOT DEFINED WARNING_FLAGS)
	set(WARNING_FLAGS
	    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra>>
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
target_compile_options(libglox INTERFACE 
	${WARNING_FLAGS}
	${LIBGLOX_COMPILE_OPTIONS}
)

# Install config
if (${SET_INSTALL_TARGETS})
	include(GNUInstallDirs)

	install(TARGETS libglox	LIBRARY 
		DESTINATION lib
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
