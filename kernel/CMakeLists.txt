add_executable( kernel
    main.cpp
    string.cpp
    gloxor/mutex.cpp
    system/danger.cpp
    system/logging.cpp
    system/font.cpp
    system/terminal.cpp
    memory/pmm.cpp
    memory/alloc.cpp
    memory/virtmem.cpp
)

target_sources(kernel
    PUBLIC
    FILE_SET kernel_cxx_modules
    TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_SOURCE_DIR}
    FILES
        system/danger.cppm
        system/logging.cppm
        system/terminal.cppm

        memory/memory.cppm
        memory/pmm.cppm
        memory/alloc.cppm
        memory/llallocator.cppm
        memory/virtmem.cppm

        system/danger.cppm
        system/logging.cppm
        system/terminal.cppm
        system/system.cppm

        gloxor/mutex.cppm
        gloxor/types.cppm
        gloxor/kinfo.cppm
        gloxor/graphics.cppm
        gloxor/gloxor.cppm
)

add_subdirectory(arch)
add_subdirectory(drivers)

set_target_properties(
    kernel PROPERTIES
    SUFFIX .img
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)


CPMAddPackage(
    NAME cxxheaders
    GIT_REPOSITORY "https://github.com/CarboSauce/cxx-headers-freestanding.git"
    GIT_TAG "c32d4d68b17d9452504de7d7e1661b1c87f49002"
    DOWNLOAD_ONLY
)

if (cxxheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cxxheaders_SOURCE_DIR}/include>
    )
    add_library(import_std)

    target_sources(import_std
        PUBLIC
        FILE_SET import_std_files
        TYPE CXX_MODULES
        BASE_DIRS ${cxxheaders_SOURCE_DIR}
        FILES
            ${cxxheaders_SOURCE_DIR}/modules/std.cc
            ${cxxheaders_SOURCE_DIR}/modules/std.compat.cc
    )
    target_include_directories(import_std
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cxxheaders_SOURCE_DIR}/include>
    )
    target_compile_options(import_std PUBLIC
        $<$<COMPILE_LANGUAGE:ASM_NASM>:${CMMN_ASM_FLAGS}>
        ${KERNEL_COMPILE_FLAGS}
        ${WARNING_FLAGS}
    )
    target_link_options(import_std PUBLIC
        ${KERNEL_LINK_FLAGS}
    )


    target_link_libraries(import_std PUBLIC libc)
    target_link_libraries(kernel PUBLIC import_std)
    target_link_libraries(libglox PUBLIC import_std)
endif()

CPMAddPackage(
    NAME cheaders
    GIT_REPOSITORY "https://github.com/osdev0/freestanding-c-hdrs-gnu.git"
    GIT_TAG "7082a7596238932f20cf0013e8448f74b53e814c"
    DOWNLOAD_ONLY
)

if (cheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cheaders_SOURCE_DIR}/${OS_ARCH}/include>
    )

    target_include_directories(import_std
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cheaders_SOURCE_DIR}/${OS_ARCH}/include>
    )

endif()

target_compile_options(kernel PRIVATE
    $<$<COMPILE_LANGUAGE:ASM_NASM>:${CMMN_ASM_FLAGS}>
    ${GLOX_COMPILE_FLAGS}
    ${WARNING_FLAGS}
)

target_include_directories(kernel PRIVATE
    ${PROJECT_SOURCE_DIR}/kernel
    ${PROJECT_SOURCE_DIR}/kernel/libarch
)

target_link_options(kernel PRIVATE
    ${KERNEL_LINK_FLAGS}
)
target_link_libraries(kernel PRIVATE
    ${LIBRARY_DEPS}
)

install(TARGETS kernel
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)
