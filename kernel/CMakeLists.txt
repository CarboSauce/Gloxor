add_executable( kernel
    main.cpp
    abi.cpp
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

if (COMPILE_TESTS)
    target_sources(kernel PUBLIC gloxor/test.cpp)
endif()

target_sources(kernel
    PUBLIC
    FILE_SET kernel_cxx_modules
    TYPE CXX_MODULES
    BASE_DIRS ${CMAKE_SOURCE_DIR}
    FILES
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
        gloxor/initgraph.cppm
)

add_subdirectory(arch)
add_subdirectory(drivers)

set_target_properties(
    kernel PROPERTIES
    SUFFIX .img
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)

if (cxxheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cxxheaders_SOURCE_DIR}/include>
    )
endif()

if (cheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cheaders_SOURCE_DIR}/${OS_ARCH}/include>
    )

endif()

CPMAddPackage(
    NAME flanterm
    GIT_REPOSITORY "https://github.com/Mintsuki/flanterm"
    VERSION 3.1.2
    DOWNLOAD_ONLY
)

if (flanterm_ADDED)
    target_include_directories(kernel
        PUBLIC $<BUILD_INTERFACE:${flanterm_SOURCE_DIR}/src>
    )
    target_sources(kernel PRIVATE
        "${flanterm_SOURCE_DIR}/src/flanterm.c"
        "${flanterm_SOURCE_DIR}/src/flanterm_backends/fb.c"
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
