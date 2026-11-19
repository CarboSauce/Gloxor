CPMAddPackage("gh:doctest/doctest@2.5.3")
CPMAddPackage(
    URI "gh:google/benchmark@1.10.0"
    OPTIONS "BENCHMARK_ENABLE_TESTING Off"
)

set(test_sources
    vector.cpp
    tuple.cpp
    result.cpp
    option.cpp
    list.cpp
    rbtree.cpp
    box.cpp
    variant.cpp
    format.cpp
    string.cpp
    hash_map.cpp
    heap_alloc.cpp
    deque.cpp
    path.cpp
    cstring.cpp
    mutex.cpp
    circular_buffer.cpp
    initgraph.cpp
    function.cpp
    mpsc_queue.cpp
    intrusive_list.cpp
    radix_tree.cpp
    buddy.cpp
)


include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
add_custom_target(tests)
foreach(t ${test_sources})
    get_filename_component(name ${t} NAME_WLE)
    add_executable(${name}_test ${t} exec_assert.cpp)
    target_link_libraries(${name}_test PRIVATE
        libglox doctest::doctest doctest_with_main
        $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
    )
    target_compile_options(${name}_test PRIVATE
        $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
        ${WARNING_FLAGS}
    )
    target_compile_features(${name}_test
        PUBLIC cxx_std_26
    )
    doctest_discover_tests(${name}_test)
    add_dependencies(tests ${name}_test)
endforeach()

add_executable(benchmarks
    benchmarks/heap_alloc.cpp
    benchmarks/mimalloc.cpp
    benchmarks/function.cpp
    benchmarks/deque.cpp
)

target_compile_features(benchmarks
    PUBLIC cxx_std_26
)

target_compile_features(doctest INTERFACE cxx_std_20)
target_compile_features(doctest_with_main INTERFACE cxx_std_20)
target_compile_features(benchmark PUBLIC cxx_std_23)


target_link_libraries(benchmarks PRIVATE libglox benchmark::benchmark mimalloc)

target_compile_options(benchmarks PRIVATE
    ${WARNING_FLAGS}
)

