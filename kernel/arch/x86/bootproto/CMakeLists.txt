set(LD_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_DISTRO}/linker.ld)
target_link_options(kernel PRIVATE
	-T${LD_SCRIPT}
)

set_target_properties(kernel PROPERTIES LINK_DEPENDS ${LD_SCRIPT})

if (PROTO_DISTRO STREQUAL "limine")
	target_sources(kernel PRIVATE
		limine/limine.cpp
	)
elseif (PROTO_DISTRO STREQUAL "stivale")
	add_subdirectory(stivale)
elseif( PROTO_DISTRO STREQUAL "egg")
	target_sources(kernel PRIVATE 
	egg/kernel_entry.asm
	)
else()
	message(FATAL_ERROR "Bootloader Protocol not supported.")
endif()
