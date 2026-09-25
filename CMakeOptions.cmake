###########
#   User customizable options
###########

set(OS_ARCH "x86_64" CACHE STRING "Architecture of OS to be compiled")
set(PROTO_DISTRO "limine" CACHE STRING "Bootloader protocol to be used")
set(OUTPUT_DIR "${CMAKE_SOURCE_DIR}/out" CACHE STRING "Use this directory to create an os image")

set(LOG_LEVEL 3 CACHE STRING "Defines default log level; Fatal = 0, Trace = 1, Debug = 2")
option(FORCE_DEBUG_CHECKS "Force debug checks, even in Release" ON)
option(USE_LTO "Compile with LTO" OFF)
option(COMPILE_TESTS "Compile Tests" ON)
option(WITH_PIC "Compile kernel with PIC" OFF)
