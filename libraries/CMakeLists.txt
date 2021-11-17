add_subdirectory(libc)
add_subdirectory(libglox)
add_subdirectory(gloxor)

add_library(library_deps INTERFACE)


# link all the libraries with all the headers and dependencies
# interface seems to be broken with handling dependencies
target_link_libraries(library_deps INTERFACE libc libgloxor libglox)
target_include_directories(library_deps INTERFACE 
    ${PROJECT_SOURCE_DIR}/libraries
)
