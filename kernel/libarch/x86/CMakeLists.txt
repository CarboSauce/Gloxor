target_sources(kernel PRIVATE 
    src/cpu.cpp
    src/paging.cpp
    # src/cpuHelpers.asm #Not needed for now
)
target_include_directories(kernel PRIVATE 
    ./inc
)
add_subdirectory(bootproto)