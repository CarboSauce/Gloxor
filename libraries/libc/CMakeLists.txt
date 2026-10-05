# Create Libc as object files
# Marking it as static composes issues during LTO
add_library(libc INTERFACE)
target_include_directories(libc SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/libraries/libc/includes
    ${PROJECT_SOURCE_DIR}/libraries/libc
)

# this shouldnt be needed but might be sometime, related to LTO

# target_link_options(libc PUBLIC
# ${KERNEL_LINK_FLAGS}
# )
