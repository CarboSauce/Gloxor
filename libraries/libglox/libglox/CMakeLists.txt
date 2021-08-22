cmake_minimum_required(VERSION 3.14)
project(libglox VERSION 1.0 LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake )


option(USE_LTO "Should we use Link time optimizations" ON)
option(COMPILE_TESTS "Enable tests?" ON)
option(SET_INSTALL_TARGETS "Should we attach installation targets?" OFF)

# set_property(GLOBAL PROPERTY 
#              CXX_STANDARD 20
# )
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
add_library(libglox STATIC
 src/algo.cpp
)

set_target_properties(libglox PROPERTIES
	PREFIX ""
)


target_include_directories(libglox PUBLIC
 ./include
)




if (NOT DEFINED WARNING_FLAGS)
	set(WARNING_FLAGS
	    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra>>
	)
endif()

if (NOT DEFINED LIBGLOX_COMPILE_OPTIONS)
	message(TRACE "No additional libglox options specified")
endif()

target_compile_options(libglox PRIVATE 
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



