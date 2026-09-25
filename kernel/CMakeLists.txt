add_executable( kernel
    main.cpp
    gloxor/mutex.cpp
    system/danger.cpp
    system/logging.cpp
    system/font.cpp
    system/terminal.cpp
    memory/pmm.cpp
    memory/alloc.cpp
    memory/virtmem.cpp
)


add_subdirectory(arch)
add_subdirectory(drivers)

set_target_properties(kernel PROPERTIES
                       SUFFIX .img
                       RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)


CPMAddPackage(
    NAME cxxheaders
    GIT_REPOSITORY "https://github.com/osdev0/freestanding-cxx-hdrs-gnu.git"
    GIT_TAG "6fc33d66b123874631cf27bc9ab80a9ab3a43dd5"
    DOWNLOAD_ONLY
)

if (cxxheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cxxheaders_SOURCE_DIR}/${OS_ARCH}/include>
    )
endif()

CPMAddPackage(
    NAME cheaders
    GIT_REPOSITORY "https://github.com/osdev0/freestanding-c-hdrs.git"
    GIT_TAG "38fed4e1e3365733ddbfa03b0a28936243ad31e9"
    DOWNLOAD_ONLY
)

if (cheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cheaders_SOURCE_DIR}/${OS_ARCH}/include>
    )
endif()


target_compile_definitions(kernel PRIVATE
    LIBGLOX_DEFAULT_ALLOCATOR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/memory/alloc.hpp"
)

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
