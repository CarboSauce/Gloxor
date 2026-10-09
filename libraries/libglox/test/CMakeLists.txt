CPMAddPackage("gh:doctest/doctest@2.5.3")

add_executable(tests
    vector.cpp
    tuple.cpp
    result.cpp
    option.cpp
    list.cpp
    rbtree.cpp
    variant.cpp
)

target_compile_features(tests
    PUBLIC cxx_std_26
)

# add_executable(module_tests
#     modules.cpp
# )
# target_link_libraries(module_tests libglox)

target_compile_features(doctest INTERFACE cxx_std_20)
target_compile_features(doctest_with_main INTERFACE cxx_std_20)

target_link_libraries(tests PRIVATE libglox doctest::doctest doctest_with_main
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
)
target_compile_options(tests PRIVATE
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
    ${WARNING_FLAGS}
)
