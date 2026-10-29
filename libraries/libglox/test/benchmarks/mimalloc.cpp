#ifndef USE_MODULES
#include "glox/alloc_types.hpp"
#include "glox/heap_alloc.hpp"
#include <atomic>
#include <benchmark/benchmark.h>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mimalloc.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>
#else
import std;
import glox;
#include <benchmark/benchmark.h>
#include <mimalloc.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

constexpr std::size_t TestPageSize = 65536; // 64 KiB Page Slabs
constexpr std::size_t TestMaxSmallSize = 2048; // 2 KiB Small Upper Bound
constexpr std::size_t TestGranularity = 8;
constexpr std::size_t TestNumBuckets = TestMaxSmallSize / TestGranularity;

static void BM_Mimalloc_SingleThreaded_FastPath(benchmark::State& state)
{
    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        void* ptr = mi_malloc(alloc_size);
        std::memset(ptr, 0xAB, alloc_size); // Touch memory payload
        benchmark::DoNotOptimize(ptr);
        mi_free(ptr);
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) * alloc_size
    );
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Mimalloc_SingleThreaded_FastPath)
    ->RangeMultiplier(2)
    ->Range(8, TestMaxSmallSize);

static void BM_Mimalloc_Multithreaded_Scalability(benchmark::State& state)
{
    const std::size_t alloc_size = 128;

    for (auto _ : state) {
        void* ptr = mi_malloc(alloc_size);
        benchmark::DoNotOptimize(ptr);
        mi_free(ptr);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Mimalloc_Multithreaded_Scalability)
    ->ThreadRange(1, static_cast<int>(std::thread::hardware_concurrency()))
    ->UseRealTime();

static void BM_Mimalloc_CrossThread_AtomicFree(benchmark::State& state)
{
    static std::atomic<void*> shared_slot { nullptr };
    const std::size_t alloc_size = 256;

    if (state.thread_index() == 0) {
        for (auto _ : state) {
            void* ptr = mi_malloc(alloc_size);
            while (shared_slot.load(std::memory_order_relaxed) != nullptr) {
                std::this_thread::yield();
            }
            shared_slot.store(ptr, std::memory_order_release);
        }
    } else {
        for (auto _ : state) {
            void* ptr = nullptr;
            while (
                (ptr = shared_slot.exchange(nullptr, std::memory_order_acquire))
                == nullptr
            ) {
                std::this_thread::yield();
            }
            mi_free(ptr);
        }
    }
}
BENCHMARK(BM_Mimalloc_CrossThread_AtomicFree)->Threads(2)->UseRealTime();

static void BM_Mimalloc_MixedSizesAndAlignments(benchmark::State& state)
{
    constexpr std::size_t sizes[] = { 16, 48, 128, 256, 512, 1024, 2048 };
    constexpr std::size_t alignments[] = { 8, 16, 32, 64, 128, 256 };

    std::size_t i = 0;
    for (auto _ : state) {
        std::size_t sz = sizes[i % std::size(sizes)];
        std::size_t align = alignments[i % std::size(alignments)];

        void* ptr = mi_malloc_aligned(sz, align);
        benchmark::DoNotOptimize(ptr);
        mi_free(ptr);

        ++i;
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Mimalloc_MixedSizesAndAlignments);

static void BM_Mimalloc_Batch_Page_Thrashing(benchmark::State& state)
{
    const std::size_t allocSize = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t batchCount = 500;
    std::vector<void*> handles(batchCount);

    for (auto _ : state) {
        for (std::size_t i = 0; i < batchCount; ++i) {
            handles[i] = mi_malloc(allocSize);
            *static_cast<volatile std::uint64_t*>(handles[i]) = i;
        }

        benchmark::DoNotOptimize(handles.data());

        for (std::size_t i = batchCount; i > 0; --i) {
            mi_free(handles[i - 1]);
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * batchCount
    );
}
BENCHMARK(BM_Mimalloc_Batch_Page_Thrashing)
    ->RangeMultiplier(4)
    ->Range(16, 2048);
