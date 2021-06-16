add_library(libglox STATIC
    src/font.cpp
    src/graphics.cpp
)

target_include_directories(libglox PUBLIC
 ./
)

target_compile_options(libglox PRIVATE 
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;${CMMN_FLAGS}>>
)


target_link_libraries(libglox PUBLIC libc)