#ifndef USE_MODULES
#include "glox/heap_alloc.hpp"
#include "glox/alloc_types.hpp"
#include <atomic>
#include <benchmark/benchmark.h>
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

constexpr std::size_t TEST_PAGE_SIZE = 65536; // 64 KiB Page Slabs
constexpr std::size_t TEST_MAX_SMALL_SIZE = 2048; // 2 KiB Small Upper Bound
constexpr std::size_t TEST_GRANULARITY = 8;
constexpr std::size_t TEST_NUM_BUCKETS = TEST_MAX_SMALL_SIZE / TEST_GRANULARITY;

template <std::size_t PageSize = TEST_PAGE_SIZE>
struct mmap_page_allocator
{
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        std::size_t totalSize = n + PageSize;
        void* rawPtr = ::mmap(
            nullptr,
            totalSize,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );

        if (rawPtr == MAP_FAILED) {
            return { nullptr, 0 };
        }

        auto addr = reinterpret_cast<std::uintptr_t>(rawPtr);
        std::uintptr_t alignedAddr = (addr + PageSize - 1) & ~(PageSize - 1);

        // Trim unaligned prefix
        std::size_t prefixSize = alignedAddr - addr;
        if (prefixSize > 0) {
            ::munmap(rawPtr, prefixSize);
        }

        // Trim unaligned suffix
        std::size_t suffixSize = totalSize - prefixSize - n;
        if (suffixSize > 0) {
            void* suffixPtr = reinterpret_cast<void*>(alignedAddr + n);
            ::munmap(suffixPtr, suffixSize);
        }

        return { reinterpret_cast<void*>(alignedAddr), n };
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
    static inline thread_local std::size_t currentTid = 0;
    static inline thread_local glox::thread_cache<TEST_NUM_BUCKETS> tcache { };

    std::size_t thread_id() const noexcept
    {
        return currentTid;
    }
    glox::thread_cache<TEST_NUM_BUCKETS>& get_tcache() const noexcept
    {
        return tcache;
    }

    static void set_thread_id(std::size_t id)
    {
        currentTid = id;
        tcache = glox::thread_cache<TEST_NUM_BUCKETS> { };
    }
};

using bench_heap_allocator = glox::heap_allocator<
    mmap_page_allocator<TEST_PAGE_SIZE>,
    mock_thread_routines,
    TEST_MAX_SMALL_SIZE,
    TEST_GRANULARITY,
    TEST_PAGE_SIZE
>;

static void BM_Glox_SingleThreaded_FastPath(benchmark::State& state)
{
    mock_thread_routines::set_thread_id(1);
    bench_heap_allocator alloc;
    const std::size_t allocSize = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        auto handle = alloc.alloc(allocSize);
        std::memset(handle.ptr, 0xAB, allocSize); // Touch memory payload
        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr, allocSize);
    }

    state.SetBytesProcessed(
        static_cast<int64_t>(state.iterations()) * allocSize
    );
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Glox_SingleThreaded_FastPath)
    ->RangeMultiplier(2)
    ->Range(8, TEST_MAX_SMALL_SIZE);

static void BM_Glox_Multithreaded_Scalability(benchmark::State& state)
{
    static bench_heap_allocator sharedAlloc;

    mock_thread_routines::set_thread_id(
        static_cast<std::size_t>(state.thread_index() + 1)
    );
    const std::size_t allocSize = 128;

    for (auto _ : state) {
        auto handle = sharedAlloc.alloc(allocSize);
        benchmark::DoNotOptimize(handle.ptr);
        sharedAlloc.dealloc(handle.ptr, allocSize);
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Glox_Multithreaded_Scalability)
    ->ThreadRange(1, static_cast<int>(std::thread::hardware_concurrency()))
    ->UseRealTime();

static void BM_Glox_CrossThread_AtomicFree(benchmark::State& state)
{
    static bench_heap_allocator alloc;
    static std::atomic<void*> sharedSlot { nullptr };
    const std::size_t allocSize = 256;

    if (state.thread_index() == 0) {
        mock_thread_routines::set_thread_id(1);
        for (auto _ : state) {
            auto handle = alloc.alloc(allocSize);
            while (sharedSlot.load(std::memory_order_relaxed) != nullptr) {
                std::this_thread::yield();
            }
            sharedSlot.store(handle.ptr, std::memory_order_release);
        }
    } else {
        mock_thread_routines::set_thread_id(2);
        for (auto _ : state) {
            void* ptr = nullptr;
            while (
                (ptr = sharedSlot.exchange(nullptr, std::memory_order_acquire))
                == nullptr
            ) {
                std::this_thread::yield();
            }
            alloc.dealloc(ptr, allocSize);
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

    const std::size_t allocSize = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t batchCount = 500;
    std::vector<glox::alloc_handle<void>> handles(batchCount);

    for (auto _ : state) {
        for (std::size_t i = 0; i < batchCount; ++i) {
            handles[i] = alloc.alloc(allocSize);
            *static_cast<volatile std::uint64_t*>(handles[i].ptr) = i;
        }

        benchmark::DoNotOptimize(handles.data());

        for (std::size_t i = batchCount; i > 0; --i) {
            alloc.dealloc(handles[i - 1].ptr, allocSize);
        }
    }

    state.SetItemsProcessed(
        static_cast<int64_t>(state.iterations()) * batchCount
    );
}
BENCHMARK(BM_Glox_Batch_Page_Thrashing)->RangeMultiplier(4)->Range(16, 2048);

BENCHMARK_MAIN();
