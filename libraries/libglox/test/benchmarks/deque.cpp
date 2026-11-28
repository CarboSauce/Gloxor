#ifndef USE_MODULES
#include "glox/deque.hpp"
#include "glox/detail/moveutils.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <numeric>
#include <random>
#include <vector>
#else
import std;
import glox;
#endif
#include <benchmark/benchmark.h>

namespace {

struct payload_64b
{
    std::array<std::uint64_t, 8> data { };

    explicit payload_64b(std::uint64_t v = 0) noexcept
    {
        data[0] = v;
        data[7] = v ^ 0x9e3779b97f4a7c15ULL;
    }
};

// 1. Sequential push_back Growth (int & 64B struct)
void BM_StdDeque_PushBack_Int(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::deque<int> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.push_back(static_cast<int>(i));
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_StdDeque_PushBack_Int)->RangeMultiplier(8)->Range(64, 1 << 18);

void BM_GloxDeque_PushBack_Int(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        glox::deque<int> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.push_back(static_cast<int>(i));
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_GloxDeque_PushBack_Int)->RangeMultiplier(8)->Range(64, 1 << 18);

void BM_StdDeque_PushBack_64B(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::deque<payload_64b> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.emplace_back(i);
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_StdDeque_PushBack_64B)->RangeMultiplier(8)->Range(64, 1 << 15);

void BM_GloxDeque_PushBack_64B(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        glox::deque<payload_64b> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.emplace_back(i);
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_GloxDeque_PushBack_64B)->RangeMultiplier(8)->Range(64, 1 << 15);

// 2. Sequential push_front Growth
void BM_StdDeque_PushFront_Int(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::deque<int> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.push_front(static_cast<int>(i));
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_StdDeque_PushFront_Int)->RangeMultiplier(8)->Range(64, 1 << 18);

void BM_GloxDeque_PushFront_Int(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        glox::deque<int> dq;
        for (std::size_t i = 0; i < n; ++i) {
            dq.push_front(static_cast<int>(i));
        }
        benchmark::DoNotOptimize(dq);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_GloxDeque_PushFront_Int)->RangeMultiplier(8)->Range(64, 1 << 18);

// 3. Steady-State FIFO Queue (push_back + pop_front)
void BM_StdDeque_FIFO_SteadyState(benchmark::State& state)
{
    std::deque<int> dq;
    for (int i = 0; i < 256; ++i) {
        dq.push_back(i);
    }

    int val = 0;
    for (auto _ : state) {
        dq.push_back(++val);
        benchmark::DoNotOptimize(dq.front());
        dq.pop_front();
    }
}
BENCHMARK(BM_StdDeque_FIFO_SteadyState);

void BM_GloxDeque_FIFO_SteadyState(benchmark::State& state)
{
    glox::deque<int> dq;
    for (int i = 0; i < 256; ++i) {
        dq.push_back(i);
    }

    int val = 0;
    for (auto _ : state) {
        dq.push_back(++val);
        benchmark::DoNotOptimize(dq.front());
        dq.pop_front();
    }
}
BENCHMARK(BM_GloxDeque_FIFO_SteadyState);

// 4. Random Access (operator[])
void BM_StdDeque_RandomAccess(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    std::deque<std::uint64_t> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(i);
    }

    std::vector<std::size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937_64 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    std::size_t cursor = 0;
    std::uint64_t sum = 0;
    for (auto _ : state) {
        sum += dq[indices[cursor]];
        cursor = (cursor + 1) & (n - 1);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_StdDeque_RandomAccess)->RangeMultiplier(16)->Range(256, 1 << 16);

void BM_GloxDeque_RandomAccess(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    glox::deque<std::uint64_t> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(i);
    }

    std::vector<std::size_t> indices(n);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937_64 rng(42);
    std::shuffle(indices.begin(), indices.end(), rng);

    std::size_t cursor = 0;
    std::uint64_t sum = 0;
    for (auto _ : state) {
        sum += dq[indices[cursor]];
        cursor = (cursor + 1) & (n - 1);
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_GloxDeque_RandomAccess)->RangeMultiplier(16)->Range(256, 1 << 16);

// 5. Sequential Range Iteration (begin() -> end())
void BM_StdDeque_Iterate(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    std::deque<std::uint64_t> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(i);
    }

    for (auto _ : state) {
        std::uint64_t sum = 0;
        for (auto const& v : dq) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_StdDeque_Iterate)->RangeMultiplier(16)->Range(256, 1 << 16);

void BM_GloxDeque_Iterate(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    glox::deque<std::uint64_t> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(i);
    }

    for (auto _ : state) {
        std::uint64_t sum = 0;
        for (auto const& v : dq) {
            sum += v;
        }
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_GloxDeque_Iterate)->RangeMultiplier(16)->Range(256, 1 << 16);

// 6. Middle Insert + Erase
void BM_StdDeque_MiddleInsertErase(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    std::deque<int> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(static_cast<int>(i));
    }

    auto const mid_offset = static_cast<std::ptrdiff_t>(n / 2);
    for (auto _ : state) {
        auto it = dq.insert(dq.begin() + mid_offset, 999);
        benchmark::DoNotOptimize(*it);
        dq.erase(dq.begin() + mid_offset);
    }
}
BENCHMARK(BM_StdDeque_MiddleInsertErase)->RangeMultiplier(4)->Range(64, 4096);

void BM_GloxDeque_MiddleInsertErase(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    glox::deque<int> dq;
    for (std::size_t i = 0; i < n; ++i) {
        dq.push_back(static_cast<int>(i));
    }

    auto const mid_offset = static_cast<std::ptrdiff_t>(n / 2);
    for (auto _ : state) {
        auto it = dq.insert(dq.begin() + mid_offset, 999);
        benchmark::DoNotOptimize(*it);
        dq.erase(dq.begin() + mid_offset);
    }
}
BENCHMARK(BM_GloxDeque_MiddleInsertErase)->RangeMultiplier(4)->Range(64, 4096);

// 7. Copy Construction + Destruction
void BM_StdDeque_Copy(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    std::deque<int> src;
    for (std::size_t i = 0; i < n; ++i) {
        src.push_back(static_cast<int>(i));
    }

    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_StdDeque_Copy)->RangeMultiplier(16)->Range(256, 1 << 16);

void BM_GloxDeque_Copy(benchmark::State& state)
{
    auto const n = static_cast<std::size_t>(state.range(0));
    glox::deque<int> src;
    for (std::size_t i = 0; i < n; ++i) {
        src.push_back(static_cast<int>(i));
    }

    for (auto _ : state) {
        auto copy = src;
        benchmark::DoNotOptimize(copy);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(n));
}
BENCHMARK(BM_GloxDeque_Copy)->RangeMultiplier(16)->Range(256, 1 << 16);

} // namespace
