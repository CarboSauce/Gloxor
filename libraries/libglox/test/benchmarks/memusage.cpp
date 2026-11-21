// Memory usage benchmarks: mimalloc vs glox::heap_allocator.
//
// Linux only (reads /proc/self/statm, uses mmap). Single-threaded.
//
// Methodology
//  * Every iteration starts cold. glox gets a fresh heap and every arena it
//    mapped is unmapped afterwards; mimalloc runs with purge_delay=0 and
//    mi_collect(true) so freed memory is handed back to the OS.
//  * Memory is measured as resident-set-size deltas (what the process really
//    holds), plus an allocator-specific "source" number: mi_process_info's
//    current_commit for mimalloc, bytes mapped by the page allocator for glox.
//  * All payloads are fully written, otherwise untouched pages never show up
//    in RSS.

#ifndef USE_MODULES
#include "glox/alloc_types.hpp"
#include "glox/heap_alloc.hpp"
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

// ---- glox plumbing
// Page allocator on top of mmap. Returns ArenaSize-aligned regions, counts the
// bytes it has mapped, and can unmap everything the heap leaked at the end of
// an iteration (heap_allocator never gives its arenas back).
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
    static inline thread_local glox::thread_cache<NUM_BUCKETS> tcache { };

    glox::thread_cache<NUM_BUCKETS>& get_tcache() const noexcept
    {
        return tcache;
    }

    static void reset_tcache()
    {
        for (std::size_t i = 0; i < NUM_BUCKETS; ++i) {
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

// ---- allocator adapters
// ---- begin() puts the
// allocator in a cold state, end() returns what it can to the OS,
// source_bytes() is the allocator's own view of what it holds.

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

static void SizeArgs(::benchmark::Benchmark* b)
{
    for (int s : { 8, 16, 24, 48, 100, 256, 1000, 2048 })
        b->Arg(s);
}

// ---- 1. Per-object overhead
// ---- Allocate ~8 MiB of live
// payload in objects of one size and compare what the process holds to what the
// caller asked for.
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

// ---- 2. Fragmentation and retention
// ---- Fill, free every other object
// (pages are half empty but still pinned), then free the rest and see how much
// memory is still held without any trim call.
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
        // Resident bytes per live byte when only half the objects remain.
        state.counters["rss_half_live_x"] = halfSum / iters / (payload / 2);
        // Still resident after everything was freed (no explicit trim).
        state.counters["retained_MiB"] = emptySum / iters / MiB;
    }
}

// ---- 3. Steady-state churn
// ---- Random alloc/free over
// a fixed set of slots with mixed sizes. arg 0 = small sizes only; arg 1 also
// mixes in 4 KiB and 20 KB blocks (above the small limit, so they use the
// large-page path).
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

MEMBENCH_REGISTER(BM_Memory_Fragmentation, mimalloc_adapter);
MEMBENCH_REGISTER(BM_Memory_Fragmentation, glox_adapter);

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

} // namespace membench

BENCHMARK_MAIN();
