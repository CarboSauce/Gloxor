target_sources(kernel PRIVATE
    src/pic8042contr.cpp
    src/pit8042.cpp
    src/cpu.cpp
    src/paging.cpp
    # src/cpuHelpers.asm #Not needed for now
)
target_include_directories(kernel PRIVATE
    ./inc
)
target_sources(kernel
    PUBLIC
    FILE_SET arch_cxx_modules
    TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/interface
    FILES
        interface/arch/addrspace.cppm
        interface/arch/archinfo.cppm
        interface/arch/cpu.cppm
        interface/arch/feature.cppm
        interface/arch/irq.cppm
        interface/arch/paging.cppm
        interface/arch/arch.cppm

        interface/asm/asm.cppm
        interface/asm/asmstubs.cppm
        interface/asm/e9.cppm
        interface/asm/gdt.cppm
        interface/asm/idt.cppm
        interface/asm/msr.cppm
        interface/asm/paging.cppm
        interface/asm/pic8042contr.cppm
        interface/asm/pit8042.cppm
)
add_subdirectory(bootproto)
