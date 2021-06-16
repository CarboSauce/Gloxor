add_library(libc STATIC  
    src/string.cpp
)
target_include_directories(libc PUBLIC
    ${PROJECT_SOURCE_DIR}/libraries/libc
    ${PROJECT_SOURCE_DIR}/libraries/libc/includes
)
target_compile_options(libc PRIVATE 
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;${CMMN_FLAGS}>>
)