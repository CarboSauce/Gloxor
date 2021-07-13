add_library(libgloxor STATIC
    src/font.cpp
    src/graphics.cpp
)

target_include_directories(libgloxor PUBLIC
 ../
)

target_compile_options(libgloxor PRIVATE 
    ${GLOX_COMPILE_FLAGS}
    ${WARNING_FLAGS}
)


target_link_libraries(libgloxor PUBLIC libglox libc )