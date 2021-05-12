add_executable( kernel 
                protos/kernel_entry.asm
                main.cpp
)

add_subdirectory(libarch)
add_subdirectory(libglox)
add_subdirectory(libsane)

set(CMMN_FLAGS -ffreestanding -fno-tree-vectorize -mgeneral-regs-only -mno-mmx -mno-sse -nostdlib)
#set(CMMN_ASM_FLAGS -f elf32)

set_target_properties(kernel PROPERTIES 
      #                 ASM_NASM_OBJ_FORMAT elf32
                       SUFFIX .img
                       LINK_FLAGS "-ffreestanding -nostdlib -T${CMAKE_CURRENT_SOURCE_DIR}/protos/linker.ld"
                       RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)



target_compile_options(kernel PRIVATE 
    $<$<COMPILE_LANGUAGE:ASM_NASM>:${CMMN_ASM_FLAGS}>
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;${CMMN_FLAGS}>>
)


target_include_directories(kernel PRIVATE 
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/libraries/libc
    ${PROJECT_SOURCE_DIR}/libraries/libc/includes
    ${PROJECT_SOURCE_DIR}/kernel/libarch
    ${PROJECT_SOURCE_DIR}/kernel/libsane
    ${PROJECT_SOURCE_DIR}/kernel/libglox
)

install(TARGETS kernel 
    DESTINATION "/"
)

