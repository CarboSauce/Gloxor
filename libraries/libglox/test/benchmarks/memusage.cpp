// Memory usage benchmarks: mimalloc vs glox::heap_allocator vs
// glox::kheap_allocator.
//
// Linux only (reads /proc/self/statm, uses mmap). Single-threaded.

#ifndef USE_MODULES
#include "glox/alloc_types.hpp"
#include "glox/heap_alloc.hpp"
#include "glox/kheap_alloc.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mimalloc.h>
#include <sys/mman.h>
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

namespace membench {

constexpr std::size_t PAGE_SIZE = 65536;
constexpr std::size_t MAX_SMALL_SIZE = 2048;
constexpr std::size_t GRANULARITY = 8;
constexpr std::size_t ARENA_SIZE = 32u * 1024 * 1024; // heap_allocator default
constexpr std::size_t MAX_MEDIUM_SIZE = 65536;
constexpr std::size_t PAYLOAD_BUDGET = 8u * 1024 * 1024; // live bytes per run
constexpr std::size_t NUM_BUCKETS = glox::heap_detail::
    total_bucket_count<MAX_SMALL_SIZE, GRANULARITY, MAX_MEDIUM_SIZE>();
constexpr std::size_t KHEAP_NUM_BUCKETS = glox::kheap_detail::
    total_bucket_count<MAX_SMALL_SIZE, GRANULARITY, MAX_MEDIUM_SIZE>();
constexpr double MiB = 1024.0 * 1024.0;

// Resident set size of this process in bytes.
inline std::int64_t rss_bytes()
{
    long total = 0, resident = 0;
    std::FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f)
        return 0;
    if (std::fscanf(f, "%ld %ld", &total, &resident) != 2)
        resident = 0;
    std::fclose(f);
    return static_cast<std::int64_t>(resident) * ::sysconf(_SC_PAGESIZE);
}

// glox (heap_alloc) plumbing
struct tracking_page_allocator
{
    static constexpr std::size_t MAX_BLOCKS = 4096;
    static inline void* blocks[MAX_BLOCKS];
    static inline std::size_t sizes[MAX_BLOCKS];
    static inline std::size_t blockCount = 0;
    static inline std::size_t mappedBytes = 0;

    static std::size_t round_os(std::size_t n)
    {
        const std::size_t ps
            = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
        return (n + ps - 1) & ~(ps - 1);
    }

    glox::alloc_handle<void> alloc(std::size_t n)
    {
        n = round_os(n);
        if (blockCount == MAX_BLOCKS)
            return { nullptr, 0 };

        const std::size_t span = n + ARENA_SIZE;
        void* raw = ::mmap(
            nullptr,
            span,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
        if (raw == MAP_FAILED)
            return { nullptr, 0 };

        const auto r = reinterpret_cast<std::uintptr_t>(raw);
        const auto a = (r + ARENA_SIZE - 1) & ~(std::uintptr_t(ARENA_SIZE) - 1);
        if (a > r)
            ::munmap(raw, a - r);
        const auto end = r + span;
        if (end > a + n)
            ::munmap(reinterpret_cast<void*>(a + n), end - (a + n));

        blocks[blockCount] = reinterpret_cast<void*>(a);
        sizes[blockCount] = n;
        ++blockCount;
        mappedBytes += n;
        return { reinterpret_cast<void*>(a), n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        if (!ptr)
            return;
        n = round_os(n);
        ::munmap(ptr, n);
        for (std::size_t i = 0; i < blockCount; ++i) {
            if (blocks[i] == ptr) {
                mappedBytes -= sizes[i];
                blocks[i] = blocks[blockCount - 1];
                sizes[i] = sizes[blockCount - 1];
                --blockCount;
                break;
            }
        }
    }

    glox::alloc_handle<void> grow(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        auto h = alloc(new_n);
        if (h.ptr && ptr) {
            std::memcpy(h.ptr, ptr, std::min(old_n, new_n));
            dealloc(ptr, old_n);
        }
        return h;
    }

    glox::alloc_handle<void> shrink(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        return grow(ptr, old_n, new_n);
    }

    static void release_all()
    {
        for (std::size_t i = 0; i < blockCount; ++i)
            ::munmap(blocks[i], sizes[i]);
        blockCount = 0;
        mappedBytes = 0;
    }
};

struct bench_thread_routines
{
    static constexpr std::size_t BUCKETS = glox::heap_detail::
        total_bucket_count<MAX_SMALL_SIZE, GRANULARITY, MAX_MEDIUM_SIZE>();
    static inline thread_local glox::thread_cache<BUCKETS> tcache { };

    glox::thread_cache<BUCKETS>& get_tcache() noexcept
    {
        return tcache;
    }

    static void reset_tcache()
    {
        for (std::size_t i = 0; i < BUCKETS; ++i) {
            tcache.pages[i] = nullptr;
            tcache.atomic_stash[i].store(nullptr, std::memory_order_relaxed);
        }
    }
};

using glox_heap = glox::heap_allocator<
    tracking_page_allocator,
    bench_thread_routines,
    MAX_SMALL_SIZE,
    GRANULARITY,
    PAGE_SIZE,
    ARENA_SIZE,
    glox::spinlock,
    MAX_MEDIUM_SIZE
>;

// kheap (kheap_alloc) plumbing
constexpr std::size_t KHEAP_PAGE_SHIFT = 16;
constexpr std::size_t KHEAP_PAGE_SIZE = std::size_t(1) << KHEAP_PAGE_SHIFT;
constexpr std::size_t KHEAP_WINDOW_BYTES = 512ull * 1024 * 1024; // 512 MiB
constexpr std::size_t KHEAP_MAX_PFNS = KHEAP_WINDOW_BYTES >> KHEAP_PAGE_SHIFT;

struct kheap_desc
{
    std::uint64_t v { 0 };

    static kheap_desc make_reserved() noexcept
    {
        return { 0 };
    }
    static kheap_desc make_buddy_free(std::size_t o) noexcept
    {
        return { (std::uint64_t(o) << 6) | 32 };
    }
    static kheap_desc make_allocated(std::size_t o) noexcept
    {
        return { std::uint64_t(o) << 6 };
    }
    bool is_buddy_free() const noexcept
    {
        return (v & 31) == 0 && (v & 32) != 0;
    }
    std::size_t buddy_order() const noexcept
    {
        return v >> 6;
    }

    static kheap_desc from_raw(std::uint64_t r) noexcept
    {
        return { r };
    }
    std::uint64_t raw() const noexcept
    {
        return v;
    }
};

struct kheap_arena_state
{
    static inline std::uintptr_t base = 0;
    static inline std::size_t bump_offset = 0;
    static inline std::size_t mapped_bytes = 0;
    static inline std::vector<kheap_desc> db;

    static void init()
    {
        if (base == 0) {
            void* p = ::mmap(
                nullptr,
                KHEAP_WINDOW_BYTES,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1,
                0
            );
            base = reinterpret_cast<std::uintptr_t>(p);
            db.resize(KHEAP_MAX_PFNS);
        }
        reset();
    }

    static void reset()
    {
        if (base != 0 && bump_offset > 0) {
            ::mmap(
                reinterpret_cast<void*>(base),
                bump_offset,
                PROT_NONE,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                -1,
                0
            );
            std::fill(
                db.begin(),
                db.begin() + (bump_offset >> KHEAP_PAGE_SHIFT),
                kheap_desc { 0 }
            );
        }
        bump_offset = 0;
        mapped_bytes = 0;
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
        return pfn < (kheap_arena_state::bump_offset >> KHEAP_PAGE_SHIFT);
    }
    void* pfn_to_virt(std::uintptr_t pfn) const noexcept
    {
        return reinterpret_cast<void*>(
            kheap_arena_state::base + (pfn << KHEAP_PAGE_SHIFT)
        );
    }
    std::uintptr_t virt_to_pfn(const void* p) const noexcept
    {
        const auto a = reinterpret_cast<std::uintptr_t>(p);
        if (a < kheap_arena_state::base)
            return KHEAP_MAX_PFNS;
        return (a - kheap_arena_state::base) >> KHEAP_PAGE_SHIFT;
    }

    static constexpr std::uint64_t TAG_BUDDY = 0x00;
    static constexpr std::uint64_t TAG_META = 0x01;
    static constexpr std::uint64_t TAG_SLAB = 0x02;
    static constexpr std::uint64_t TAG_LARGE = 0x03;
    static constexpr std::uint64_t TAG_LARGE_TAIL = 0x04;
};

struct kheap_page_allocator
{
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        n = (n + KHEAP_PAGE_SIZE - 1) & ~(KHEAP_PAGE_SIZE - 1);
        if (kheap_arena_state::bump_offset + n > KHEAP_WINDOW_BYTES)
            return { nullptr, 0 };

        void* ptr = reinterpret_cast<void*>(
            kheap_arena_state::base + kheap_arena_state::bump_offset
        );
        void* res = ::mmap(
            ptr,
            n,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
            -1,
            0
        );
        if (res == MAP_FAILED)
            return { nullptr, 0 };

        kheap_arena_state::bump_offset += n;
        kheap_arena_state::mapped_bytes += n;
        return { ptr, n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        if (!ptr)
            return;
        n = (n + KHEAP_PAGE_SIZE - 1) & ~(KHEAP_PAGE_SIZE - 1);
        ::madvise(ptr, n, MADV_DONTNEED);
        kheap_arena_state::mapped_bytes -= n;
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
        old_n = (old_n + KHEAP_PAGE_SIZE - 1) & ~(KHEAP_PAGE_SIZE - 1);
        new_n = (new_n + KHEAP_PAGE_SIZE - 1) & ~(KHEAP_PAGE_SIZE - 1);
        if (old_n > new_n) {
            ::madvise(
                static_cast<char*>(ptr) + new_n, old_n - new_n, MADV_DONTNEED
            );
            kheap_arena_state::mapped_bytes -= (old_n - new_n);
        }
        return { ptr, new_n };
    }
};

struct kheap_thread_routines
{
    static inline thread_local glox::kthread_cache<KHEAP_NUM_BUCKETS>
        tcache { };

    glox::kthread_cache<KHEAP_NUM_BUCKETS>& get_tcache() noexcept
    {
        return tcache;
    }

    static void reset_tcache()
    {
        for (std::size_t i = 0; i < KHEAP_NUM_BUCKETS; ++i) {
            tcache.pages[i] = nullptr;
            tcache.atomic_stash[i].store(nullptr, std::memory_order_relaxed);
        }
        tcache.meta_free_list = nullptr;
    }
};

using kheap_alloc_t = glox::kheap_allocator<
    kheap_page_allocator,
    kheap_pfndb,
    kheap_thread_routines,
    glox::spinlock,
    KHEAP_PAGE_SHIFT,
    MAX_SMALL_SIZE,
    GRANULARITY,
    MAX_MEDIUM_SIZE
>;

// allocator adapters
struct mimalloc_adapter
{
    void begin()
    {
        mi_option_set(mi_option_purge_delay, 0);
        mi_collect(true);
    }
    void* alloc(std::size_t n)
    {
        return mi_malloc(n);
    }
    void free(void* p, std::size_t)
    {
        mi_free(p);
    }
    void end()
    {
        mi_collect(true);
    }

    std::int64_t source_bytes()
    {
        std::size_t elapsed = 0, user = 0, sys = 0, rss = 0, peakRss = 0;
        std::size_t commit = 0, peakCommit = 0, faults = 0;
        mi_process_info(
            &elapsed, &user, &sys, &rss, &peakRss, &commit, &peakCommit, &faults
        );
        return static_cast<std::int64_t>(commit);
    }
};

struct glox_adapter
{
    glox_heap heap;

    void begin()
    {
        tracking_page_allocator::release_all();
        bench_thread_routines::reset_tcache();
    }
    void* alloc(std::size_t n)
    {
        return heap.alloc(n).ptr;
    }
    void free(void* p, std::size_t)
    {
        heap.dealloc(p);
    }
    void end()
    {
        bench_thread_routines::reset_tcache();
        tracking_page_allocator::release_all();
    }

    std::int64_t source_bytes()
    {
        return static_cast<std::int64_t>(tracking_page_allocator::mappedBytes);
    }
};

struct kheap_adapter
{
    kheap_alloc_t heap;

    void begin()
    {
        kheap_arena_state::init();
        kheap_thread_routines::reset_tcache();
    }
    void* alloc(std::size_t n)
    {
        return heap.alloc(n).ptr;
    }
    void free(void* p, std::size_t)
    {
        heap.dealloc(p);
    }
    void end()
    {
        heap.release_cache(kheap_thread_routines::tcache);
        kheap_thread_routines::reset_tcache();
        kheap_arena_state::reset();
    }

    std::int64_t source_bytes()
    {
        return static_cast<std::int64_t>(kheap_arena_state::mapped_bytes);
    }
};

static void SizeArgs(::benchmark::Benchmark* b)
{
    for (int s : { 8, 16, 24, 48, 100, 256, 1000, 2048 })
        b->Arg(s);
}

// 1. Per-object overhead
template <class Adapter>
static void BM_Memory_Overhead(benchmark::State& state)
{
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    const std::size_t count = std::max<std::size_t>(1, PAYLOAD_BUDGET / size);
    const double payload
        = static_cast<double>(count) * static_cast<double>(size);
    std::vector<void*> ptrs(count, nullptr);
    double rssSum = 0, srcSum = 0;

    for (auto _ : state) {
        Adapter a;
        a.begin();
        const std::int64_t rss0 = rss_bytes();
        const std::int64_t src0 = a.source_bytes();

        std::size_t done = 0;
        for (; done < count; ++done) {
            void* p = a.alloc(size);
            if (!p)
                break;
            std::memset(p, 0xAB, size);
            ptrs[done] = p;
        }

        rssSum += static_cast<double>(rss_bytes() - rss0);
        srcSum += static_cast<double>(a.source_bytes() - src0);

        for (std::size_t i = 0; i < done; ++i)
            a.free(ptrs[i], size);
        a.end();

        if (done != count) {
            state.SkipWithError("allocation failed");
            break;
        }
    }

    const double iters = static_cast<double>(state.iterations());
    if (iters > 0) {
        state.counters["payload_MiB"] = payload / MiB;
        state.counters["rss_B_per_obj"] = rssSum / iters / double(count);
        state.counters["rss_overhead_x"] = rssSum / iters / payload;
        state.counters["source_overhead_x"] = srcSum / iters / payload;
    }
}

// 2. Fragmentation and retention
template <class Adapter>
static void BM_Memory_Fragmentation(benchmark::State& state)
{
    const std::size_t size = static_cast<std::size_t>(state.range(0));
    const std::size_t count = std::max<std::size_t>(2, PAYLOAD_BUDGET / size);
    const double payload
        = static_cast<double>(count) * static_cast<double>(size);
    std::vector<void*> ptrs(count, nullptr);
    double fullSum = 0, halfSum = 0, emptySum = 0;

    for (auto _ : state) {
        Adapter a;
        a.begin();
        const std::int64_t rss0 = rss_bytes();

        bool ok = true;
        for (std::size_t i = 0; i < count && ok; ++i) {
            ptrs[i] = a.alloc(size);
            if (!ptrs[i]) {
                ok = false;
                break;
            }
            std::memset(ptrs[i], 0xAB, size);
        }
        fullSum += static_cast<double>(rss_bytes() - rss0);

        for (std::size_t i = 1; i < count && ok; i += 2) {
            a.free(ptrs[i], size);
            ptrs[i] = nullptr;
        }
        halfSum += static_cast<double>(rss_bytes() - rss0);

        for (std::size_t i = 0; i < count && ok; i += 2) {
            a.free(ptrs[i], size);
            ptrs[i] = nullptr;
        }
        emptySum += static_cast<double>(rss_bytes() - rss0);

        a.end();
        if (!ok) {
            state.SkipWithError("allocation failed");
            break;
        }
    }

    const double iters = static_cast<double>(state.iterations());
    if (iters > 0) {
        state.counters["rss_full_x"] = fullSum / iters / payload;
        state.counters["rss_half_live_x"] = halfSum / iters / (payload / 2);
        state.counters["retained_MiB"] = emptySum / iters / MiB;
    }
}

// 3. Steady-state churn
template <class Adapter>
static void BM_Memory_Churn(benchmark::State& state)
{
    constexpr std::size_t slots = 20000;
    constexpr std::size_t ops = 1000000;
    constexpr std::size_t sizes[]
        = { 16, 24, 48, 64, 96, 128, 256, 512, 1000, 2048, 4096, 20000 };
    const std::size_t nsizes = state.range(0) != 0 ? 12 : 10;

    std::vector<void*> ptr(slots);
    std::vector<std::uint32_t> sz(slots);
    double peakRssSum = 0, peakLiveSum = 0;

    for (auto _ : state) {
        std::fill(ptr.begin(), ptr.end(), nullptr);
        std::fill(sz.begin(), sz.end(), 0u);

        Adapter a;
        a.begin();
        const std::int64_t rss0 = rss_bytes();

        std::uint64_t rng = 0x9E3779B97F4A7C15ull;
        std::int64_t live = 0, peakLive = 0, peakRss = 0;
        bool ok = true;

        for (std::size_t op = 0; op < ops && ok; ++op) {
            rng ^= rng << 13;
            rng ^= rng >> 7;
            rng ^= rng << 17;
            const std::size_t slot = rng % slots;

            if (ptr[slot]) {
                a.free(ptr[slot], sz[slot]);
                live -= sz[slot];
                ptr[slot] = nullptr;
            } else {
                const std::size_t s = sizes[(rng >> 32) % nsizes];
                void* p = a.alloc(s);
                if (!p) {
                    ok = false;
                    break;
                }
                std::memset(p, 0x5A, s);
                ptr[slot] = p;
                sz[slot] = static_cast<std::uint32_t>(s);
                live += static_cast<std::int64_t>(s);
                peakLive = std::max(peakLive, live);
            }

            if ((op & 0xFFFF) == 0xFFFF)
                peakRss = std::max(peakRss, rss_bytes() - rss0);
        }
        peakRss = std::max(peakRss, rss_bytes() - rss0);
        peakRssSum += static_cast<double>(peakRss);
        peakLiveSum += static_cast<double>(peakLive);

        for (std::size_t i = 0; i < slots; ++i)
            if (ptr[i])
                a.free(ptr[i], sz[i]);
        a.end();

        if (!ok) {
            state.SkipWithError("allocation failed");
            break;
        }
    }

    const double iters = static_cast<double>(state.iterations());
    if (iters > 0) {
        state.counters["peak_live_MiB"] = peakLiveSum / iters / MiB;
        state.counters["peak_rss_MiB"] = peakRssSum / iters / MiB;
        state.counters["rss_over_live_x"] = peakRssSum / peakLiveSum;
    }
}

#define MEMBENCH_REGISTER(fn, adapter) \
    BENCHMARK_TEMPLATE(fn, adapter)    \
        ->Apply(SizeArgs)              \
        ->Iterations(5)                \
        ->Unit(benchmark::kMillisecond)

MEMBENCH_REGISTER(BM_Memory_Overhead, mimalloc_adapter);
MEMBENCH_REGISTER(BM_Memory_Overhead, glox_adapter);
MEMBENCH_REGISTER(BM_Memory_Overhead, kheap_adapter);

MEMBENCH_REGISTER(BM_Memory_Fragmentation, mimalloc_adapter);
MEMBENCH_REGISTER(BM_Memory_Fragmentation, glox_adapter);
MEMBENCH_REGISTER(BM_Memory_Fragmentation, kheap_adapter);

BENCHMARK_TEMPLATE(BM_Memory_Churn, mimalloc_adapter)
    ->Arg(0)
    ->Arg(1)
    ->Iterations(3)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Memory_Churn, glox_adapter)
    ->Arg(0)
    ->Arg(1)
    ->Iterations(3)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_Memory_Churn, kheap_adapter)
    ->Arg(0)
    ->Arg(1)
    ->Iterations(3)
    ->Unit(benchmark::kMillisecond);

} // namespace membench

BENCHMARK_MAIN();
