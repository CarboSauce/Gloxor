#ifndef USE_MODULES
#include "glox/heap_alloc.hpp"
#include "glox/alloc_types.hpp"
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

template <std::size_t PageSize = TestPageSize>
struct mmap_page_allocator
{
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        std::size_t total_size = n + PageSize;
        void* raw_ptr = ::mmap(
            nullptr,
            total_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );

        if (raw_ptr == MAP_FAILED) {
            return { nullptr, 0 };
        }

        auto addr = reinterpret_cast<std::uintptr_t>(raw_ptr);
        std::uintptr_t aligned_addr = (addr + PageSize - 1) & ~(PageSize - 1);

        // Trim unaligned prefix
        std::size_t prefix_size = aligned_addr - addr;
        if (prefix_size > 0) {
            ::munmap(raw_ptr, prefix_size);
        }

        // Trim unaligned suffix
        std::size_t suffix_size = total_size - prefix_size - n;
        if (suffix_size > 0) {
            void* suffix_ptr = reinterpret_cast<void*>(aligned_addr + n);
            ::munmap(suffix_ptr, suffix_size);
        }

        return { reinterpret_cast<void*>(aligned_addr), n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        if (ptr) {
            ::munmap(ptr, n);
        }
    }

    glox::alloc_handle<void> grow(void*, std::size_t, std::size_t)
    {
        return { nullptr, 0 };
    }
    glox::alloc_handle<void> shrink(void*, std::size_t, std::size_t)
    {
        return { nullptr, 0 };
    }
};

struct mock_thread_routines
{
    static inline thread_local std::size_t current_tid = 0;
    static inline thread_local glox::thread_cache<TestNumBuckets> tcache { };

    std::size_t thread_id() const noexcept
    {
        return current_tid;
    }
    glox::thread_cache<TestNumBuckets>& get_tcache() const noexcept
    {
        return tcache;
    }

    static void set_thread_id(std::size_t id)
    {
        current_tid = id;
        tcache = glox::thread_cache<TestNumBuckets> { };
    }
};

using bench_heap_allocator = glox::heap_allocator<
    mmap_page_allocator<TestPageSize>,
    mock_thread_routines,
    TestMaxSmallSize,
    TestGranularity,
    TestPageSize
>;

static void BM_Glox_SingleThreaded_FastPath(benchmark::State& state)
{
    mock_thread_routines::set_thread_id(1);
    bench_heap_allocator alloc;
    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        auto handle = alloc.alloc(alloc_size);
        std::memset(handle.ptr, 0xAB, alloc_size); // Touch memory payload
        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr, alloc_size);
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) * alloc_size
    );
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Glox_SingleThreaded_FastPath)
    ->RangeMultiplier(2)
    ->Range(8, TestMaxSmallSize);

static void BM_Glox_Multithreaded_Scalability(benchmark::State& state)
{
    static bench_heap_allocator shared_alloc;

    mock_thread_routines::set_thread_id(
        static_cast<std::size_t>(state.thread_index() + 1)
    );
    const std::size_t alloc_size = 128;

    for (auto _ : state) {
        auto handle = shared_alloc.alloc(alloc_size);
        benchmark::DoNotOptimize(handle.ptr);
        shared_alloc.dealloc(handle.ptr, alloc_size);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Glox_Multithreaded_Scalability)
    ->ThreadRange(1, static_cast<int>(std::thread::hardware_concurrency()))
    ->UseRealTime();

static void BM_Glox_CrossThread_AtomicFree(benchmark::State& state)
{
    static bench_heap_allocator alloc;
    static std::atomic<void*> shared_slot { nullptr };
    const std::size_t alloc_size = 256;

    if (state.thread_index() == 0) {
        mock_thread_routines::set_thread_id(1);
        for (auto _ : state) {
            auto handle = alloc.alloc(alloc_size);
            while (shared_slot.load(std::memory_order_relaxed) != nullptr) {
                std::this_thread::yield();
            }
            shared_slot.store(handle.ptr, std::memory_order_release);
        }
    } else {
        mock_thread_routines::set_thread_id(2);
        for (auto _ : state) {
            void* ptr = nullptr;
            while (
                (ptr = shared_slot.exchange(nullptr, std::memory_order_acquire))
                == nullptr
            ) {
                std::this_thread::yield();
            }
            alloc.dealloc(ptr, alloc_size);
        }
    }
}
BENCHMARK(BM_Glox_CrossThread_AtomicFree)->Threads(2)->UseRealTime();

static void BM_Glox_MixedSizesAndAlignments(benchmark::State& state)
{
    mock_thread_routines::set_thread_id(1);
    bench_heap_allocator alloc;

    constexpr std::size_t sizes[] = { 16, 48, 128, 256, 512, 1024, 2048 };
    constexpr std::size_t alignments[] = { 8, 16, 32, 64, 128, 256 };

    std::size_t i = 0;
    for (auto _ : state) {
        std::size_t sz = sizes[i % std::size(sizes)];
        std::size_t align = alignments[i % std::size(alignments)];

        auto handle = alloc.alloc(sz, align);
        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr, sz, align);

        ++i;
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Glox_MixedSizesAndAlignments);

static void BM_Glox_Batch_Page_Thrashing(benchmark::State& state)
{
    mock_thread_routines::set_thread_id(1);
    ::bench_heap_allocator alloc;

    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t batch_count = 500;
    std::vector<glox::alloc_handle<void>> handles(batch_count);

    for (auto _ : state) {
        for (std::size_t i = 0; i < batch_count; ++i) {
            handles[i] = alloc.alloc(alloc_size);
            *static_cast<volatile std::uint64_t*>(handles[i].ptr) = i;
        }

        benchmark::DoNotOptimize(handles.data());

        for (std::size_t i = batch_count; i > 0; --i) {
            alloc.dealloc(handles[i - 1].ptr, alloc_size);
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * batch_count
    );
}
BENCHMARK(BM_Glox_Batch_Page_Thrashing)->RangeMultiplier(4)->Range(16, 2048);

BENCHMARK_MAIN();
