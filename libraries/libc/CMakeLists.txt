# Create Libc as object files
# Marking it as static composes issues during LTO
add_library(libc OBJECT
    src/string.cpp
)

if (performConfigureTests)
    check_cxx_source_compiles(
        "#include <string.h>" is_system_stringh_present
    )

    if (is_system_stringh_present)
        set(libcDirectory 
        "${PROJECT_SOURCE_DIR}/libraries/libc/includes"
        )
    endif()
endif()
target_include_directories(libc PUBLIC
    ${PROJECT_SOURCE_DIR}/libraries/libc
    ${libcDirectory}
)


target_compile_options(libc PRIVATE
${GLOX_COMPILE_FLAGS}
${WARNING_FLAGS}
)

# this shouldnt be needed but might be sometime, related to LTO 

# target_link_options(libc PUBLIC
# ${KERNEL_LINK_FLAGS}
# )