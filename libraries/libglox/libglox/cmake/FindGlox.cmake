#[=[
FindGlox
-------

Finds the Glox library.

-----------------------
This module provides the following imported targets, if found:

Glox::Glox
	The Glox library

-----------------------
This will define the following variables:

GLOX_FOUND
	True if the system has the Glox library.
GLOX_INCLUDE_DIRS
	Include directories needed to use Glox.
GLOX_LIBRARIES
	Libraries needed to link to Glox.

-----------------------
Following Flags control behavior

GlOX_ROOT
	Glox root directory
GLOX_LIBRARY
	Hint for finding library location
GLOX_FIND_SHARED
	Set if shared library needs to be found

#]=]


# Cmake acts weird when searching libraries for not major compilers
# We handle it ourself

list(APPEND CMAKE_FIND_LIBRARY_PREFIXES lib "")
if (GLOX_FIND_SHARED)
	list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .dll .dll.a .so .dylib)
else()
	list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)
endif()
list(REMOVE_DUPLICATES CMAKE_FIND_LIBRARY_SUFFIXES)

find_path(GLOX_INCLUDE_DIR NAMES glox/glox.hpp
	PATHS 
		${GLOX_ROOT}
		${GLOX_ROOT}/inc
		${GLOX_ROOT}/include
)

find_library(GLOX_LIBRARY NAMES glox
	PATHS 
		${GLOX_ROOT}
		${GLOX_LIBRARY_HINT}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Glox
	FOUND_VAR 
		GLOX_FOUND
	REQUIRED_VARS 
		GLOX_LIBRARY
		GLOX_INCLUDE_DIR
)

if (GLOX_FOUND)
	 set(GLOX_LIBRARIES ${GLOX_LIBRARY})
	 set(GLOX_INCLUDE_DIRS ${GLOX_INCLUDE_DIR})
endif()

if (GLOX_FOUND AND NOT TARGET Glox::Glox)
	add_library(Glox::Glox UNKNOWN IMPORTED)
	set_target_properties(Glox::Glox PROPERTIES
		IMPORTED_LOCATION "${GLOX_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${GLOX_INCLUDE_DIR}"
	)
endif()
