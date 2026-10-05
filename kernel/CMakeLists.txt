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


add_subdirectory(arch)
add_subdirectory(drivers)

set_target_properties(kernel PROPERTIES
                       SUFFIX .img
                       RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)


CPMAddPackage(
    NAME cxxheaders
    GIT_REPOSITORY "https://github.com/CarboSauce/cxx-headers-freestanding.git"
    GIT_TAG "a5dd2fdd90139a5fd7c8363711de6c2f95c4c7aa"
    DOWNLOAD_ONLY
)

if (cxxheaders_ADDED)
    target_include_directories(kernel
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${cxxheaders_SOURCE_DIR}/include>
    )
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
