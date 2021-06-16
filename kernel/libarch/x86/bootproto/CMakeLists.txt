target_link_options(kernel PRIVATE
-T${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_DISTRO}/linker.ld
)


if (PROTO_DISTRO STREQUAL "stivale")
    add_subdirectory(stivale)
elseif( PROTO_DISTRO STREQUAL "egg")
    target_sources(kernel PRIVATE 
    egg/kernel_entry.asm
    )
else()
    message(FATAL_ERROR "Bootloader Protocol not supported.")
endif()