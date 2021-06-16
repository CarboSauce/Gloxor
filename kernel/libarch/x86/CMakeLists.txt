target_sources(kernel PRIVATE 
    src/cpu.cpp
)
target_include_directories(kernel PRIVATE 
    ./inc
)
add_subdirectory(bootproto)