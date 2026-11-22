#ifndef USE_MODULES
#include "glox/kheap_alloc.hpp"
#include "glox/alloc_types.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <sys/mman.h>
#include <thread>
#include <vector>
#else
import std;
import glox;
#include <barrier>
#include <benchmark/benchmark.h>
#include <sys/mman.h>
#endif

constexpr std::size_t PAGE_SHIFT = 16; // 64 KiB pages
constexpr std::size_t PAGE_SIZE = std::size_t(1) << PAGE_SHIFT;
constexpr std::size_t WINDOW_BYTES
    = 8ull * 1024 * 1024 * 1024; // 512 MiB virtual arena
constexpr std::size_t MAX_PFNS = WINDOW_BYTES >> PAGE_SHIFT;

constexpr std::size_t MAX_SMALL_SIZE = 2048;
constexpr std::size_t GRANULARITY = 8;
constexpr std::size_t MAX_MEDIUM_SIZE = 65536;

constexpr std::size_t NUM_BUCKETS = glox::kheap_detail::
    total_bucket_count<MAX_SMALL_SIZE, GRANULARITY, MAX_MEDIUM_SIZE>();

// -----------------------------------------------------------------------------
// Kheap PFN descriptor & virtual arena
// -----------------------------------------------------------------------------

struct kheap_desc
{
    std::uint64_t v { 0 };

    static kheap_desc make_reserved() noexcept
    {
        return { 0 };
    }

    static kheap_desc make_buddy_free(std::size_t order) noexcept
    {
        return { (std::uint64_t(order) << 6) | 32 };
    }

    static kheap_desc make_allocated(std::size_t order) noexcept
    {
        return { std::uint64_t(order) << 6 };
    }

    bool is_buddy_free() const noexcept
    {
        return (v & 31) == 0 && (v & 32) != 0;
    }

    std::size_t buddy_order() const noexcept
    {
        return v >> 6;
    }

    static kheap_desc from_raw(std::uint64_t raw) noexcept
    {
        return { raw };
    }

    std::uint64_t raw() const noexcept
    {
        return v;
    }
};

struct kheap_arena_state
{
    static inline glox::spinlock lock { };
    static inline std::uintptr_t base = 0;
    static inline std::atomic<std::size_t> bump_offset { 0 };
    static inline std::atomic<std::size_t> mapped_bytes { 0 };
    static inline std::vector<kheap_desc> db;

    static void ensure_reserved()
    {
        if (base != 0)
            return;

        void* p = ::mmap(
            nullptr,
            WINDOW_BYTES,
            PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,
            -1,
            0
        );

        if (p == MAP_FAILED)
            throw std::bad_alloc();

        base = reinterpret_cast<std::uintptr_t>(p);
        db.resize(MAX_PFNS);
    }

    static void reset()
    {
        std::lock_guard guard(lock);
        ensure_reserved();

        const std::size_t used = bump_offset.load(std::memory_order_relaxed);
        if (used > 0) {
            ::mmap(
                reinterpret_cast<void*>(base),
                used,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
                -1,
                0
            );

            std::fill(
                db.begin(), db.begin() + (used >> PAGE_SHIFT), kheap_desc { }
            );
        }

        bump_offset.store(0, std::memory_order_relaxed);
        mapped_bytes.store(0, std::memory_order_relaxed);
    }
};

struct kheap_pfndb
{
    kheap_desc& operator[](std::uintptr_t pfn) noexcept
    {
        return kheap_arena_state::db[pfn];
    }

    const kheap_desc& operator[](std::uintptr_t pfn) const noexcept
    {
        return kheap_arena_state::db[pfn];
    }

    bool is_valid(std::uintptr_t pfn) const noexcept
    {
        return pfn
             < (kheap_arena_state::bump_offset.load(std::memory_order_acquire)
                 >> PAGE_SHIFT);
    }

    void* pfn_to_virt(std::uintptr_t pfn) const noexcept
    {
        return reinterpret_cast<void*>(
            kheap_arena_state::base + (pfn << PAGE_SHIFT)
        );
    }

    std::uintptr_t virt_to_pfn(const void* p) const noexcept
    {
        const auto addr = reinterpret_cast<std::uintptr_t>(p);

        if (addr < kheap_arena_state::base)
            return MAX_PFNS;

        return (addr - kheap_arena_state::base) >> PAGE_SHIFT;
    }

    static constexpr std::uint64_t TAG_BUDDY = 0x00;
    static constexpr std::uint64_t TAG_META = 0x01;
    static constexpr std::uint64_t TAG_SLAB = 0x02;
    static constexpr std::uint64_t TAG_LARGE = 0x03;
    static constexpr std::uint64_t TAG_LARGE_TAIL = 0x04;
};

// -----------------------------------------------------------------------------
// Thread-safe Kheap backing page allocator
// -----------------------------------------------------------------------------

struct kheap_page_allocator
{
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        std::lock_guard guard(kheap_arena_state::lock);
        n = (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        const std::size_t offset
            = kheap_arena_state::bump_offset.load(std::memory_order_relaxed);
        if (offset + n > WINDOW_BYTES)
            return { nullptr, 0 };

        void* ptr = reinterpret_cast<void*>(kheap_arena_state::base + offset);

        void* result = ::mmap(
            ptr,
            n,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );

        if (result == MAP_FAILED)
            return { nullptr, 0 };

        kheap_arena_state::bump_offset.store(
            offset + n, std::memory_order_release
        );
        kheap_arena_state::mapped_bytes.fetch_add(n, std::memory_order_relaxed);

        return { ptr, n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        if (!ptr)
            return;

        std::lock_guard guard(kheap_arena_state::lock);
        n = (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        const auto addr = reinterpret_cast<std::uintptr_t>(ptr);
        const std::size_t offset
            = kheap_arena_state::bump_offset.load(std::memory_order_relaxed);

        if (addr + n == kheap_arena_state::base + offset) {
            ::mmap(
                ptr,
                n,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
                -1,
                0
            );
            kheap_arena_state::bump_offset.store(
                offset - n, std::memory_order_release
            );
        } else {
            ::madvise(ptr, n, MADV_DONTNEED);
        }

        kheap_arena_state::mapped_bytes.fetch_sub(n, std::memory_order_relaxed);
    }

    glox::alloc_handle<void> grow(void*, std::size_t, std::size_t)
    {
        return { nullptr, 0 };
    }

    glox::alloc_handle<void> shrink(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        std::lock_guard guard(kheap_arena_state::lock);
        old_n = (old_n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        new_n = (new_n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        if (old_n > new_n) {
            ::madvise(
                static_cast<char*>(ptr) + new_n, old_n - new_n, MADV_DONTNEED
            );
            kheap_arena_state::mapped_bytes -= (old_n - new_n);
        }

        return { ptr, new_n };
    }
};

// Per-thread Kheap cache

struct kheap_thread_routines
{
    static inline thread_local std::size_t current_tid = 0;
    static inline thread_local glox::kthread_cache<NUM_BUCKETS> tcache { };

    std::size_t thread_id() const noexcept
    {
        return current_tid;
    }

    glox::kthread_cache<NUM_BUCKETS>& get_tcache() noexcept
    {
        return tcache;
    }

    static void set_thread_id(std::size_t id) noexcept
    {
        current_tid = id;
    }
};

using bench_kheap_allocator = glox::kheap_allocator<
    kheap_page_allocator,
    kheap_pfndb,
    kheap_thread_routines,
    glox::spinlock,
    PAGE_SHIFT,
    MAX_SMALL_SIZE,
    GRANULARITY,
    MAX_MEDIUM_SIZE
>;

// -----------------------------------------------------------------------------
// 1. Single-threaded fast path
// -----------------------------------------------------------------------------

static void BM_KHeap_SingleThreaded_FastPath(benchmark::State& state)
{
    kheap_arena_state::reset();
    kheap_thread_routines::set_thread_id(1);
    bench_kheap_allocator alloc;

    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        auto handle = alloc.alloc(alloc_size);
        if (!handle.ptr) {
            state.SkipWithError("Kheap allocation failed");
            break;
        }

        std::memset(handle.ptr, 0xAB, alloc_size);
        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr);
    }

    alloc.release_cache(kheap_thread_routines::tcache);

    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations()) * alloc_size
    );
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_KHeap_SingleThreaded_FastPath)
    ->RangeMultiplier(2)
    ->Range(8, MAX_SMALL_SIZE);

// -----------------------------------------------------------------------------
// 2. Multithreaded scalability
// -----------------------------------------------------------------------------

static void BM_KHeap_Multithreaded_Scalability(benchmark::State& state)
{
    static bench_kheap_allocator* shared_alloc = nullptr;
    static std::atomic<int> active_threads { 0 };

    if (state.thread_index() == 0) {
        kheap_arena_state::reset();
        shared_alloc = new bench_kheap_allocator();
        active_threads.store(state.threads(), std::memory_order_release);
    }

    while (active_threads.load(std::memory_order_acquire) != state.threads()) {
        std::this_thread::yield();
    }

    kheap_thread_routines::set_thread_id(
        static_cast<std::size_t>(state.thread_index() + 1)
    );

    const std::size_t alloc_size = 128;

    for (auto _ : state) {
        auto handle = shared_alloc->alloc(alloc_size);
        if (!handle.ptr) {
            state.SkipWithError("Kheap allocation failed");
            break;
        }

        benchmark::DoNotOptimize(handle.ptr);
        shared_alloc->dealloc(handle.ptr);
    }

    shared_alloc->release_cache(kheap_thread_routines::tcache);

    if (active_threads.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete shared_alloc;
        shared_alloc = nullptr;
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_KHeap_Multithreaded_Scalability)
    ->ThreadRange(
        1,
        static_cast<int>(std::max(1u, std::thread::hardware_concurrency()))
    )
    ->UseRealTime();

// -----------------------------------------------------------------------------
// 3. Cross-thread allocation and free
// -----------------------------------------------------------------------------

static void BM_KHeap_CrossThread_AtomicFree(benchmark::State& state)
{
    static bench_kheap_allocator* shared_alloc = nullptr;
    static std::atomic<void*> shared_slot { nullptr };
    static std::atomic<bool> failed { false };
    static std::barrier sync_barrier(2);

    constexpr std::size_t alloc_size = 256;

    if (state.thread_index() == 0) {
        kheap_arena_state::reset();
        shared_alloc = new bench_kheap_allocator();
        shared_slot.store(nullptr, std::memory_order_relaxed);
        failed.store(false, std::memory_order_relaxed);
    }

    sync_barrier.arrive_and_wait();

    kheap_thread_routines::set_thread_id(
        static_cast<std::size_t>(state.thread_index() + 1)
    );

    if (state.thread_index() == 0) {
        for (auto _ : state) {
            if (failed.load(std::memory_order_acquire))
                break;

            auto handle = shared_alloc->alloc(alloc_size);
            if (!handle.ptr) {
                failed.store(true, std::memory_order_release);
                state.SkipWithError("Kheap allocation failed");
                break;
            }

            while (shared_slot.load(std::memory_order_acquire) != nullptr) {
                if (failed.load(std::memory_order_acquire))
                    break;
                std::this_thread::yield();
            }

            if (failed.load(std::memory_order_acquire)) {
                shared_alloc->dealloc(handle.ptr);
                break;
            }

            shared_slot.store(handle.ptr, std::memory_order_release);
        }
    } else {
        for (auto _ : state) {
            void* ptr = nullptr;
            while (
                (ptr = shared_slot.exchange(nullptr, std::memory_order_acq_rel))
                == nullptr
            ) {
                if (failed.load(std::memory_order_acquire))
                    break;
                std::this_thread::yield();
            }

            if (failed.load(std::memory_order_acquire) && !ptr)
                break;

            if (ptr)
                shared_alloc->dealloc(ptr);
        }
    }

    sync_barrier.arrive_and_wait();
    shared_alloc->release_cache(kheap_thread_routines::tcache);
    sync_barrier.arrive_and_wait();

    if (state.thread_index() == 0) {
        delete shared_alloc;
        shared_alloc = nullptr;
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_KHeap_CrossThread_AtomicFree)->Threads(2)->UseRealTime();

// -----------------------------------------------------------------------------
// 4. Mixed sizes and alignments
// -----------------------------------------------------------------------------

static void BM_KHeap_MixedSizesAndAlignments(benchmark::State& state)
{
    kheap_arena_state::reset();
    kheap_thread_routines::set_thread_id(1);
    bench_kheap_allocator alloc;

    constexpr std::array<std::size_t, 12> sizes {
        16, 48, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536
    };
    constexpr std::array<std::size_t, 6> alignments { 8, 16, 32, 64, 128, 256 };

    std::size_t i = 0;
    for (auto _ : state) {
        const std::size_t sz = sizes[i % sizes.size()];
        const std::size_t align = alignments[i % alignments.size()];

        auto handle = alloc.alloc(sz, align);
        if (!handle.ptr) {
            state.SkipWithError("Kheap allocation failed");
            break;
        }

        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr);
        ++i;
    }

    alloc.release_cache(kheap_thread_routines::tcache);

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_KHeap_MixedSizesAndAlignments);

// -----------------------------------------------------------------------------
// 5. Batch allocation and page/cache pressure
// -----------------------------------------------------------------------------

static void BM_KHeap_Batch_Page_Thrashing(benchmark::State& state)
{
    kheap_arena_state::reset();
    kheap_thread_routines::set_thread_id(1);
    bench_kheap_allocator alloc;

    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));
    constexpr std::size_t batch_count = 500;
    std::vector<void*> handles(batch_count, nullptr);

    for (auto _ : state) {
        std::size_t allocated = 0;
        for (; allocated < batch_count; ++allocated) {
            auto handle = alloc.alloc(alloc_size);
            if (!handle.ptr) {
                state.SkipWithError("Kheap allocation failed");
                break;
            }

            handles[allocated] = handle.ptr;
            *static_cast<volatile std::uint64_t*>(handles[allocated])
                = allocated;
        }

        benchmark::DoNotOptimize(handles.data());

        for (std::size_t i = allocated; i > 0; --i) {
            alloc.dealloc(handles[i - 1]);
            handles[i - 1] = nullptr;
        }

        if (allocated != batch_count)
            break;
    }

    alloc.release_cache(kheap_thread_routines::tcache);

    state.SetItemsProcessed(
        static_cast<std::int64_t>(state.iterations()) * batch_count
    );
}
BENCHMARK(BM_KHeap_Batch_Page_Thrashing)->RangeMultiplier(4)->Range(16, 2048);

// 6. Large allocations (buddy / multi-page path)

static void BM_KHeap_LargeAllocations(benchmark::State& state)
{
    kheap_arena_state::reset();
    kheap_thread_routines::set_thread_id(1);
    bench_kheap_allocator alloc;

    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        auto handle = alloc.alloc(alloc_size);
        if (!handle.ptr) {
            state.SkipWithError("Kheap allocation failed");
            break;
        }

        std::memset(handle.ptr, 0xEF, alloc_size);
        benchmark::DoNotOptimize(handle.ptr);
        alloc.dealloc(handle.ptr);
    }

    alloc.release_cache(kheap_thread_routines::tcache);

    state.SetBytesProcessed(
        static_cast<std::int64_t>(state.iterations()) * alloc_size
    );
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}
BENCHMARK(BM_KHeap_LargeAllocations)
    ->RangeMultiplier(2)
    ->Range(128 * 1024, 4 * 1024 * 1024);
