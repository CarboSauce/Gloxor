cmake_minimum_required(VERSION 4.4.3)
###########
# all ugly variables that had to be set
# thanks to cmake not supporting osdev that much
# feel free to add other hack variables
###########
set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")
set(CMAKE_SYSTEM_NAME "Generic")
###
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS ON)

project(Gloxor VERSION 1.0 LANGUAGES C CXX ASM_NASM)

include(CMakeOptions.cmake)
include(CheckCXXSourceCompiles)

if (WITH_PIC)
    #set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    list(APPEND KERNEL_COMPILE_FLAGS -fpie)
    list(APPEND KERNEL_LINK_FLAGS -pie)
    #list(APPEND KERNEL_LINK_FLAGS)
else()
    set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
    list(APPEND KERNEL_COMPILE_FLAGS -mcmodel=kernel -fno-pic -fno-pie )
    list(APPEND KERNEL_LINK_FLAGS -no-pie)
endif()

add_compile_definitions(LOG_LEVEL=${LOG_LEVEL})
if (COMPILE_TESTS)
    add_compile_definitions(GLOXTESTING)
endif()
if (FORCE_DEBUG_CHECKS OR CMAKE_BUILD_TYPE EQUAL "DEBUG")
    add_compile_definitions(DEBUG)
endif()


set(WARNING_FLAGS
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wno-interrupt-service-routine>>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:-Wall;-Wextra;-Wno-interrupt-service-routine>>
)
set(GLOX_COMPILE_FLAGS
    $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:${KERNEL_COMPILE_FLAGS}>>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU,AppleClang>:$<BUILD_INTERFACE:${KERNEL_COMPILE_FLAGS}>>
)

if (USE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ${USE_LTO})
endif()

add_subdirectory(libraries)
add_subdirectory(kernel)


include(ExternalProject)

set(LIMINE_VERSION "12.9.0")
set(LIMINE_DIR ${OUTPUT_DIR}/limine-binary)
set(LIMINE_ARCHIVE ${OUTPUT_DIR}/limine-${LIMINE_VERSION}-binary.zip)
set(LIMINE_URL "https://github.com/Limine-bootloader/Limine/releases/download/v${LIMINE_VERSION}/limine-binary.zip" "Limine URL to download")

add_custom_target(limine
    COMMAND ${CMAKE_COMMAND}
        -DLIMINE_URL=${LIMINE_URL}
        -DOUTPUT_DIR=${OUTPUT_DIR}
        -DLIMINE_ARCHIVE=${LIMINE_ARCHIVE}
        -DLIMINE_VERSION=${LIMINE_VERSION}
        -DLIMINE_DIR=${LIMINE_DIR}
        -P "${CMAKE_SOURCE_DIR}/scripts/build-limine.cmake"
    OUTPUT "${LIMINE_DIR}/limine"

    VERBATIM
)

set(OS_ISO "${OUTPUT_DIR}/os.iso")
add_custom_command(
    OUTPUT "${OS_ISO}"

    COMMAND "${CMAKE_SOURCE_DIR}/scripts/mklimineimg.sh"
        "$<TARGET_FILE:kernel>"
        "${OUTPUT_DIR}"
        "${LIMINE_DIR}"

    DEPENDS
        kernel
        limine
        "${CMAKE_SOURCE_DIR}/scripts/mklimineimg.sh"
        "${OUTPUT_DIR}/limine.conf"

    VERBATIM
)

add_custom_target(iso DEPENDS "${OS_ISO}" )
add_dependencies(iso kernel limine)

set(QEMU_COMMAND "qemu-system-x86_64" CACHE STRING "Command used to launch QEMU")
set(QEMU_ARGS "" CACHE STRING "Additional args passed to QEMU")

add_custom_target(run-qemu
    COMMAND
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-qemu.sh
        "${QEMU_COMMAND}"
        "${OS_ISO}"
        "${QEMU_ARGS}"
    DEPENDS
        iso
    USES_TERMINAL
)
