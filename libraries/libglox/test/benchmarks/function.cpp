#ifndef USE_MODULES
#include "glox/function.hpp"
#include "glox/detail/moveutils.hpp"
#include <array>
#include <cstdint>
#include <functional>
#else
import std;
import glox;
#endif
#include <benchmark/benchmark.h>

namespace glox {
[[noreturn]] void exec_assert(const char* message, std::source_location loc)
{
    std::printf(
        "Assert hit\nMessage:\nFunction: %s\n%s\nFile: %s\nLine: %d\n",
        message,
        loc.function_name(),
        loc.file_name(),
        loc.line()
    );
    std::exit(1);
}
} // namespace glox

namespace {

// ============================================================================
// Test Callables of Varying Capture Sizes
// ============================================================================
std::uint64_t free_fn(std::uint64_t x) noexcept
{
    return x + 1;
}

// 8 bytes: Fits in both std::function (16B SBO) and glox::function (24B SBO)
struct capture_8b
{
    std::uint64_t a;
    std::uint64_t operator()(std::uint64_t x) const noexcept
    {
        return a + x;
    }
};

// 16 bytes: SBO boundary (16B payload + 8B vptr = 24B in glox::function)
struct capture_16b
{
    std::uint64_t a;
    std::uint64_t b;
    std::uint64_t operator()(std::uint64_t x) const noexcept
    {
        return (a ^ b) + x;
    }
};

// 64 bytes: Spills to heap in both std::function and glox::function (zero-cost
// in function_ref)
struct capture_64b
{
    std::array<std::uint64_t, 8> data;
    std::uint64_t operator()(std::uint64_t x) const noexcept
    {
        return data[0] + data[7] + x;
    }
};

// ============================================================================
// 1. Construction + Destruction (8B SBO)
// ============================================================================
void BM_StdFunction_CtorDtor_8B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        std::function<std::uint64_t(std::uint64_t)> fn = capture_8b { ++i };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_StdFunction_CtorDtor_8B);

void BM_GloxFunction_CtorDtor_8B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        glox::function<std::uint64_t(std::uint64_t)> fn = capture_8b { ++i };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_GloxFunction_CtorDtor_8B);

void BM_GloxFunctionRef_Ctor_8B(benchmark::State& state)
{
    capture_8b cb { 42 };
    for (auto _ : state) {
        glox::function_ref<std::uint64_t(std::uint64_t)> ref = cb;
        benchmark::DoNotOptimize(ref);
    }
}
BENCHMARK(BM_GloxFunctionRef_Ctor_8B);

// ============================================================================
// 2. Construction + Destruction (16B SBO Boundary)
// ============================================================================
void BM_StdFunction_CtorDtor_16B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        std::function<std::uint64_t(std::uint64_t)> fn
            = capture_16b { ++i, 42 };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_StdFunction_CtorDtor_16B);

void BM_GloxFunction_CtorDtor_16B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        glox::function<std::uint64_t(std::uint64_t)> fn
            = capture_16b { ++i, 42 };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_GloxFunction_CtorDtor_16B);

void BM_GloxFunctionRef_Ctor_16B(benchmark::State& state)
{
    capture_16b cb { 13, 37 };
    for (auto _ : state) {
        glox::function_ref<std::uint64_t(std::uint64_t)> ref = cb;
        benchmark::DoNotOptimize(ref);
    }
}
BENCHMARK(BM_GloxFunctionRef_Ctor_16B);

// ============================================================================
// 3. Construction + Destruction (64B Heap vs Zero-Alloc Ref)
// ============================================================================
void BM_StdFunction_CtorDtor_64B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        std::function<std::uint64_t(std::uint64_t)> fn
            = capture_64b { { ++i, 1, 2, 3, 4, 5, 6, 7 } };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_StdFunction_CtorDtor_64B);

void BM_GloxFunction_CtorDtor_64B(benchmark::State& state)
{
    std::uint64_t i = 0;
    for (auto _ : state) {
        glox::function<std::uint64_t(std::uint64_t)> fn
            = capture_64b { { ++i, 1, 2, 3, 4, 5, 6, 7 } };
        benchmark::DoNotOptimize(fn);
    }
}
BENCHMARK(BM_GloxFunction_CtorDtor_64B);

void BM_GloxFunctionRef_Ctor_64B(benchmark::State& state)
{
    capture_64b cb { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    for (auto _ : state) {
        glox::function_ref<std::uint64_t(std::uint64_t)> ref = cb;
        benchmark::DoNotOptimize(ref);
    }
}
BENCHMARK(BM_GloxFunctionRef_Ctor_64B);

// ============================================================================
// 4. Invocation Throughput (Free Function Pointer)
// ============================================================================
void BM_StdFunction_Invoke_FreeFn(benchmark::State& state)
{
    std::function<std::uint64_t(std::uint64_t)> fn = &free_fn;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_StdFunction_Invoke_FreeFn);

void BM_GloxFunction_Invoke_FreeFn(benchmark::State& state)
{
    glox::function<std::uint64_t(std::uint64_t)> fn = &free_fn;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunction_Invoke_FreeFn);

void BM_GloxFunctionRef_Invoke_FreeFn(benchmark::State& state)
{
    glox::function_ref<std::uint64_t(std::uint64_t)> ref = &free_fn;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = ref(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunctionRef_Invoke_FreeFn);

// ============================================================================
// 5. Invocation Throughput (16B SBO)
// ============================================================================
void BM_StdFunction_Invoke_16B(benchmark::State& state)
{
    std::function<std::uint64_t(std::uint64_t)> fn = capture_16b { 13, 37 };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_StdFunction_Invoke_16B);

void BM_GloxFunction_Invoke_16B(benchmark::State& state)
{
    glox::function<std::uint64_t(std::uint64_t)> fn = capture_16b { 13, 37 };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunction_Invoke_16B);

void BM_GloxFunctionRef_Invoke_16B(benchmark::State& state)
{
    capture_16b cb { 13, 37 };
    glox::function_ref<std::uint64_t(std::uint64_t)> ref = cb;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = ref(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunctionRef_Invoke_16B);

// ============================================================================
// 6. Invocation Throughput (64B Heap vs Ref)
// ============================================================================
void BM_StdFunction_Invoke_64B(benchmark::State& state)
{
    std::function<std::uint64_t(std::uint64_t)> fn
        = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_StdFunction_Invoke_64B);

void BM_GloxFunction_Invoke_64B(benchmark::State& state)
{
    glox::function<std::uint64_t(std::uint64_t)> fn
        = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = fn(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunction_Invoke_64B);

void BM_GloxFunctionRef_Invoke_64B(benchmark::State& state)
{
    capture_64b cb { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    glox::function_ref<std::uint64_t(std::uint64_t)> ref = cb;
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = ref(x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunctionRef_Invoke_64B);

// ============================================================================
// 7. Pass-by-Value Callback Overhead (Construct + Invoke + Destroy)
// ============================================================================
[[gnu::noinline]] std::uint64_t sink_std_fn(
    std::function<std::uint64_t(std::uint64_t)> fn,
    std::uint64_t x
)
{
    return fn(x);
}

[[gnu::noinline]] std::uint64_t sink_glox_fn(
    glox::function<std::uint64_t(std::uint64_t)> fn,
    std::uint64_t x
)
{
    return fn(x);
}

[[gnu::noinline]] std::uint64_t sink_glox_ref(
    glox::function_ref<std::uint64_t(std::uint64_t)> ref,
    std::uint64_t x
)
{
    return ref(x);
}

void BM_StdFunction_PassByValue_64B(benchmark::State& state)
{
    capture_64b cb { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = sink_std_fn(cb, x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_StdFunction_PassByValue_64B);

void BM_GloxFunction_PassByValue_64B(benchmark::State& state)
{
    capture_64b cb { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = sink_glox_fn(cb, x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunction_PassByValue_64B);

void BM_GloxFunctionRef_PassByValue_64B(benchmark::State& state)
{
    capture_64b cb { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    std::uint64_t x = 1;
    for (auto _ : state) {
        x = sink_glox_ref(cb, x);
        benchmark::DoNotOptimize(x);
    }
}
BENCHMARK(BM_GloxFunctionRef_PassByValue_64B);

// ============================================================================
// 8. Copy Construction + Destruction (16B SBO)
// ============================================================================
void BM_StdFunction_Copy_16B(benchmark::State& state)
{
    const std::function<std::uint64_t(std::uint64_t)> src
        = capture_16b { 9, 42 };
    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_StdFunction_Copy_16B);

void BM_GloxFunction_Copy_16B(benchmark::State& state)
{
    const glox::function<std::uint64_t(std::uint64_t)> src
        = capture_16b { 9, 42 };
    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_GloxFunction_Copy_16B);

void BM_GloxFunctionRef_Copy_16B(benchmark::State& state)
{
    capture_16b cb { 9, 42 };
    const glox::function_ref<std::uint64_t(std::uint64_t)> src = cb;
    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_GloxFunctionRef_Copy_16B);

// ============================================================================
// 9. Copy Construction + Destruction (64B Heap)
// ============================================================================
void BM_StdFunction_Copy_64B(benchmark::State& state)
{
    const std::function<std::uint64_t(std::uint64_t)> src
        = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_StdFunction_Copy_64B);

void BM_GloxFunction_Copy_64B(benchmark::State& state)
{
    const glox::function<std::uint64_t(std::uint64_t)> src
        = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
}
BENCHMARK(BM_GloxFunction_Copy_64B);

// ============================================================================
// 10. Move Construction + Destruction (16B SBO)
// ============================================================================
void BM_StdFunction_Move_16B(benchmark::State& state)
{
    for (auto _ : state) {
        std::function<std::uint64_t(std::uint64_t)> a = capture_16b { 7, 11 };
        auto b = std::move(a);
        benchmark::DoNotOptimize(b);
    }
}
BENCHMARK(BM_StdFunction_Move_16B);

void BM_GloxFunction_Move_16B(benchmark::State& state)
{
    for (auto _ : state) {
        glox::function<std::uint64_t(std::uint64_t)> a = capture_16b { 7, 11 };
        auto b = RVALUE(a);
        benchmark::DoNotOptimize(b);
    }
}
BENCHMARK(BM_GloxFunction_Move_16B);

// ============================================================================
// 11. Move Construction + Destruction (64B Heap)
// ============================================================================
void BM_StdFunction_Move_64B(benchmark::State& state)
{
    for (auto _ : state) {
        std::function<std::uint64_t(std::uint64_t)> a
            = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
        auto b = std::move(a);
        benchmark::DoNotOptimize(b);
    }
}
BENCHMARK(BM_StdFunction_Move_64B);

void BM_GloxFunction_Move_64B(benchmark::State& state)
{
    for (auto _ : state) {
        glox::function<std::uint64_t(std::uint64_t)> a
            = capture_64b { { 1, 2, 3, 4, 5, 6, 7, 8 } };
        auto b = RVALUE(a);
        benchmark::DoNotOptimize(b);
    }
}
BENCHMARK(BM_GloxFunction_Move_64B);

} // namespace
