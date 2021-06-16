add_library(stivale STATIC 
    stivale2.c kernel_entry.asm
)

target_compile_options(stivale PRIVATE
    $<$<COMPILE_LANGUAGE:ASM_NASM>:${CMMN_ASM_FLAGS}>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;${CMMN_FLAGS}>>
    
)
target_include_directories(stivale PRIVATE
    ${PROJECT_SOURCE_DIR}/kernel/
)

target_link_libraries(kernel PRIVATE stivale)