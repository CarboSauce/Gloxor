set(LIBGLOX_COMPILE_OPTIONS ${GLOX_COMPILE_FLAGS})
set(COMPILE_TESTS OFF)
function(readgloxsubdir status)
   # this approach is fairly naive
   if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libglox/CMakeLists.txt)
      message(DEBUG "\t${CMAKE_CURRENT_SOURCE_DIR}/libglox/CMakeLists.txt found.")
      add_subdirectory(libglox)
      if (NOT TARGET libglox)
         message(FATAL_ERROR "\tLibGlox subdirectory detected, but libglox target was not created."
         "\n\tMake sure that everything is correctly set up."
         )
      endif()
	   target_link_libraries(libglox INTERFACE libc)
      #set_target_properties(libglox PROPERTIES INTERFACE_LINK_DEPENDS "$<LINK_ONLY:libc>")
      set(${status} TRUE PARENT_SCOPE)
   else()
      message(DEBUG "\tLibglox directory does not exist.")
      set(${status} FALSE PARENT_SCOPE)
   endif()
endfunction()

message(DEBUG "Looking for libglox subdirectory")
readgloxsubdir(isgloxfound)

if (${isgloxfound})
   message(DEBUG "Found libglox in subdirectory!")
else()
   message(DEBUG "Looking for Glox package using find_package")
   # If we are here, it means that there is no libglox library present
   find_package(Glox)
   if (GLOX_FOUND)
      message(DEBUG "\tLibglox has been found!")
      add_library(libglox INTERFACE)
      target_link_libraries(libglox INTERFACE Glox::Glox)
      return()
   endif()
   message(FATAL_ERROR "LibGlox subdirectory failed to process")
endif()

add_subdirectory(libc)

#add_library(library_deps INTERFACE)

# link all the libraries with all the headers and dependencies
# interface seems to be broken with handling dependencies
#target_link_libraries(library_deps INTERFACE libglox libc)
set(LIBRARY_DEPS libglox libc PARENT_SCOPE)