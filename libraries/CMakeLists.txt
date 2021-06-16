add_subdirectory(libc)
add_subdirectory(libglox)

add_library(library_deps INTERFACE)
target_link_libraries(library_deps INTERFACE libglox libc)
target_include_directories(library_deps INTERFACE 
    ${PROJECT_SOURCE_DIR}/libraries
)

