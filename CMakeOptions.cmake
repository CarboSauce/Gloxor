###########
#   User customizable options
###########

set(OS_ARCH "x86_64" CACHE STRING "Architecture of OS to be compiled")
set(PROTO_DISTRO "stivale" CACHE STRING "Bootloader protocol to be used")
list(APPEND CMMN_FLAGS 
# Used to make linker be willing to put our code in high memory
# alternatively we can use PIE and PIC magic
# red zone is x86_64 abi thing, in kernel code it needs to be disabled because of interrupts
-mno-red-zone
-fno-stack-protector
-fno-exceptions -fno-rtti -ffreestanding -fbuiltin -fno-threadsafe-statics -mgeneral-regs-only -mno-mmx -mno-sse -mno-sse2 -nostdlib -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=/)
list(APPEND KERNEL_LINK_FLAGS -ffreestanding -nostdlib)

set(LOG_LEVEL 3 CACHE STRING "Defines default log level; Fatal = 0, Trace = 1, Debug = 2")
option(FORCE_DEBUG "Force debug checks, even in Release" ON)
option(USE_LTO "Compile with LTO" OFF)
option(COMPILE_TESTS "Compile Tests" ON)
option(WITH_PIC "Compile kernel with PIC" OFF)
