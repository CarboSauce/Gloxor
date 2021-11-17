target_sources(kernel PRIVATE 
    stivale2.cpp
    #kernel_entry.asm
)

# target_compile_options(stivale PRIVATE
# ${GLOX_COMPILE_FLAGS}
# ${WARNING_FLAGS}
# )
# target_include_directories(stivale PRIVATE
#     ${PROJECT_SOURCE_DIR}/kernel/
# )
# target_link_options(stivale PRIVATE 
# ${KERNEL_LINK_FLAGS}
# # )
# target_link_libraries(stivale PRIVATE libc)
# target_link_libraries(kernel PRIVATE stivale) 