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
#add_subdirectory(system)
#add_subdirectory(gloxor)
#add_subdirectory(memory)

set_target_properties(kernel PROPERTIES 
                       SUFFIX .img
                       RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)
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
