CPMAddPackage("gh:doctest/doctest@2.5.3")
CPMAddPackage(
    URI "gh:google/benchmark@1.10.0"
    OPTIONS "BENCHMARK_ENABLE_TESTING Off"
)

add_executable(tests
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
)

add_executable(benchmarks
    benchmarks/heap_alloc.cpp
    benchmarks/mimalloc.cpp
)

target_compile_features(tests
    PUBLIC cxx_std_26
)
target_compile_features(benchmarks
    PUBLIC cxx_std_26
)

target_compile_features(doctest INTERFACE cxx_std_20)
target_compile_features(doctest_with_main INTERFACE cxx_std_20)
target_compile_features(benchmark INTERFACE cxx_std_20)

target_link_libraries(tests PRIVATE libglox doctest::doctest doctest_with_main
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
)
target_compile_options(tests PRIVATE
    $<$<BOOL:${LIBGLOX_SANITIZE}>:-fsanitize=address,undefined>
    ${WARNING_FLAGS}
)

target_link_libraries(benchmarks PRIVATE libglox benchmark::benchmark mimalloc)

target_compile_options(benchmarks PRIVATE
    ${WARNING_FLAGS}
)
