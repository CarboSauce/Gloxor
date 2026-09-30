function(glox_add_test param)
    add_executable(${param} ${param}Test.cpp)
    target_compile_options(${param}
        PRIVATE $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address>
    )
    target_link_libraries(${param} PRIVATE libglox
        $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address>
    )
endfunction()

CPMAddPackage("gh:doctest/doctest@2.5.3")

add_executable(tests
    vector.cpp tuple.cpp result.cpp
)
target_link_libraries(tests PRIVATE libglox doctest::doctest doctest_with_main
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
)
target_compile_options(tests PRIVATE
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
    ${WARNING_FLAGS}
)
