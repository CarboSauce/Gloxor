#ifndef USE_MODULES
#include "glox/heap_alloc.hpp"
#include "glox/alloc_types.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"

namespace {

constexpr std::size_t TEST_PAGE_SIZE = 65536;
constexpr std::size_t TEST_MAX_SMALL_SIZE = 2048;
constexpr std::size_t TEST_MAX_MEDIUM_SIZE = 65536;
constexpr std::size_t TEST_GRANULARITY = 8;
constexpr std::size_t TEST_NUM_BUCKETS = glox::heap_detail::total_bucket_count<
    TEST_MAX_SMALL_SIZE,
    TEST_GRANULARITY,
    TEST_MAX_MEDIUM_SIZE
>();
// Small arena keeps the tests cheap: 16 slices, 1 of them metadata,
// 1..2 slices for medium pages, 2..4 slices for large spans, and anything
// above 4 slices (256 KiB) becomes a dedicated "huge" arena.
constexpr std::size_t TEST_ARENA_SIZE = 1024 * 1024;
constexpr std::size_t TEST_HUGE_SIZE = 1024 * 1024;

extern "C"
{
extern int posix_memalign(
    void** __memptr,
    std::size_t __alignment,
    std::size_t __size
);
extern void free(void* p);
}

// 1. Mock page allocator returning ArenaSize-aligned memory.
struct mock_page_allocator
{
    static inline bool simulate_failure = false;

    glox::alloc_handle<void> alloc(std::size_t n)
    {
        if (simulate_failure)
            return { nullptr, 0 };

        void* ptr = nullptr;
        if (posix_memalign(&ptr, TEST_ARENA_SIZE, n) != 0)
            return { nullptr, 0 };
        return { ptr, n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        (void)n;
        if (ptr)
            free(ptr);
    }

    glox::alloc_handle<void> grow(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        if (simulate_failure)
            return { nullptr, 0 };

        auto handle = alloc(new_n);
        if (handle.ptr && ptr) {
            std::memcpy(handle.ptr, ptr, std::min(old_n, new_n));
            dealloc(ptr, old_n);
        }
        return handle;
    }

    glox::alloc_handle<void> shrink(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        return grow(ptr, old_n, new_n);
    }
};

// A page allocator that is only PageSize-aligned (never ArenaSize-aligned), to
// exercise the heap's over-allocate-and-align path.
struct offset_page_allocator
{
    glox::alloc_handle<void> alloc(std::size_t n)
    {
        void* base = nullptr;
        if (posix_memalign(&base, TEST_ARENA_SIZE, n + TEST_PAGE_SIZE) != 0)
            return { nullptr, 0 };
        return { static_cast<char*>(base) + TEST_PAGE_SIZE, n };
    }

    void dealloc(void* ptr, std::size_t)
    {
        if (ptr)
            free(static_cast<char*>(ptr) - TEST_PAGE_SIZE);
    }

    glox::alloc_handle<void> grow(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        auto handle = alloc(new_n);
        if (handle.ptr && ptr) {
            std::memcpy(handle.ptr, ptr, std::min(old_n, new_n));
            dealloc(ptr, old_n);
        }
        return handle;
    }

    glox::alloc_handle<void> shrink(
        void* ptr,
        std::size_t old_n,
        std::size_t new_n
    )
    {
        return grow(ptr, old_n, new_n);
    }
};

// 2. Per-thread caches. Page ownership is the address of the cache, so a second
//    std::thread (with its own thread_local cache) is automatically "remote".
struct mock_thread_routines
{
    static inline thread_local glox::thread_cache<TEST_NUM_BUCKETS> tcache { };

    glox::thread_cache<TEST_NUM_BUCKETS>& get_tcache() const noexcept
    {
        return tcache;
    }

    // Forget any pages left over from a previous test's heap instance.
    static void reset_tcache()
    {
        for (std::size_t i = 0; i < TEST_NUM_BUCKETS; ++i) {
            tcache.pages[i] = nullptr;
            tcache.atomic_stash[i].store(nullptr, std::memory_order_relaxed);
        }
    }
};

// 3. Mutex that only counts lock() calls (single-threaded tests).
struct counting_mutex
{
    static inline std::size_t lock_calls = 0;
    void lock() noexcept
    {
        ++lock_calls;
    }
    void unlock() noexcept { }
};

using test_heap_allocator = glox::heap_allocator<
    mock_page_allocator,
    mock_thread_routines,
    TEST_MAX_SMALL_SIZE,
    TEST_GRANULARITY,
    TEST_PAGE_SIZE,
    TEST_ARENA_SIZE,
    glox::spinlock,
    TEST_MAX_MEDIUM_SIZE
>;

using offset_heap_allocator = glox::heap_allocator<
    offset_page_allocator,
    mock_thread_routines,
    TEST_MAX_SMALL_SIZE,
    TEST_GRANULARITY,
    TEST_PAGE_SIZE,
    TEST_ARENA_SIZE,
    glox::spinlock,
    TEST_MAX_MEDIUM_SIZE
>;

using counting_heap_allocator = glox::heap_allocator<
    mock_page_allocator,
    mock_thread_routines,
    TEST_MAX_SMALL_SIZE,
    TEST_GRANULARITY,
    TEST_PAGE_SIZE,
    TEST_ARENA_SIZE,
    counting_mutex,
    TEST_MAX_MEDIUM_SIZE
>;

static_assert(test_heap_allocator::num_buckets == TEST_NUM_BUCKETS);

void fill_pattern(void* p, std::size_t n)
{
    auto* b = static_cast<unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
        b[i] = static_cast<unsigned char>(i % 251);
}

bool check_pattern(const void* p, std::size_t n)
{
    auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
        if (b[i] != static_cast<unsigned char>(i % 251))
            return false;
    return true;
}

std::uintptr_t page_of(const void* p)
{
    return reinterpret_cast<std::uintptr_t>(p) & ~(TEST_PAGE_SIZE - 1);
}

} // namespace

TEST_CASE("heap_allocator basic small allocation & deallocation")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Single allocation and size-less deallocation")
    {
        auto handle = alloc.alloc(32);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= 32);
        REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % 8 == 0);
        alloc.dealloc(handle.ptr);
    }

    SUBCASE("Freed blocks are reused once the page free list runs dry")
    {
        // Freed blocks go to the page's local_free list and are merged back
        // into the allocation list only when it is empty (as in mimalloc), so
        // reuse is not immediate LIFO. 32-byte blocks: 2048 fit in one page.
        constexpr std::size_t per_page = TEST_PAGE_SIZE / 32;
        std::vector<void*> blocks;
        for (std::size_t i = 0; i < per_page; ++i) {
            auto h = alloc.alloc(32);
            REQUIRE(h.ptr != nullptr);
            blocks.push_back(h.ptr);
        }

        alloc.dealloc(blocks[0]);

        // Page is exhausted, so the slow path collects local_free.
        auto again = alloc.alloc(32);
        REQUIRE(again.ptr == blocks[0]);

        for (std::size_t i = 1; i < blocks.size(); ++i)
            alloc.dealloc(blocks[i]);
        alloc.dealloc(again.ptr);
    }

    SUBCASE("Multiple allocations within same bucket")
    {
        constexpr std::size_t count = 10;
        glox::alloc_handle<void> handles[count];

        for (std::size_t i = 0; i < count; ++i) {
            handles[i] = alloc.alloc(64);
            REQUIRE(handles[i].ptr != nullptr);
            REQUIRE(handles[i].count >= 64);
        }
        for (std::size_t i = 0; i < count; ++i)
            for (std::size_t j = i + 1; j < count; ++j)
                REQUIRE(handles[i].ptr != handles[j].ptr);
        for (std::size_t i = 0; i < count; ++i)
            alloc.dealloc(handles[i].ptr);
    }

    SUBCASE("usable_size reports block capacity")
    {
        auto h = alloc.alloc(20);
        REQUIRE(alloc.usable_size(h.ptr) == h.count);
        REQUIRE(alloc.usable_size(h.ptr) >= 20);
        alloc.dealloc(h.ptr);
        REQUIRE(alloc.usable_size(nullptr) == 0);
    }

    SUBCASE("Page metadata is not inside the page: blocks start page-aligned")
    {
        auto h = alloc.alloc(2048);
        REQUIRE(h.ptr != nullptr);
        // First block of a fresh page sits at the page start (no header).
        REQUIRE(reinterpret_cast<std::uintptr_t>(h.ptr) % TEST_PAGE_SIZE == 0);
        alloc.dealloc(h.ptr);
    }
}

TEST_CASE("heap_allocator medium size classes")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Logarithmic sub-bins bound internal fragmentation below 20%")
    {
        const std::size_t sizes[]
            = { 2056, 3072, 4096, 6000, 10000, 20000, 32768, 50000, 65536 };

        for (std::size_t sz : sizes) {
            auto h = alloc.alloc(sz);
            REQUIRE(h.ptr != nullptr);
            REQUIRE(h.count >= sz);
            REQUIRE(alloc.usable_size(h.ptr) == h.count);
            // Worst-case overhead for 4 sub-bins per octave is < 25% of
            // requested size (< 20% of allocated block size).
            REQUIRE((h.count - sz) * 5 < h.count);
            fill_pattern(h.ptr, sz);
            REQUIRE(check_pattern(h.ptr, sz));
            alloc.dealloc(h.ptr);
        }
    }

    SUBCASE("Multi-slice medium pages pack multiple blocks without overlap")
    {
        // 10000 B rounds up to 10240 B (5 * 2048); target 8 blocks => 2 slices
        // (131072 B), capacity = 12 blocks.
        constexpr std::size_t count = 12;
        glox::alloc_handle<void> handles[count];

        for (std::size_t i = 0; i < count; ++i) {
            handles[i] = alloc.alloc(10000);
            REQUIRE(handles[i].ptr != nullptr);
            REQUIRE(handles[i].count == 10240);
            REQUIRE(alloc.usable_size(handles[i].ptr) == 10240);
            fill_pattern(handles[i].ptr, 10000);
        }

        // All 12 blocks come from a single 2-slice medium page.
        const auto base = reinterpret_cast<std::uintptr_t>(handles[0].ptr);
        REQUIRE(base % TEST_PAGE_SIZE == 0);
        for (std::size_t i = 0; i < count; ++i) {
            const auto addr = reinterpret_cast<std::uintptr_t>(handles[i].ptr);
            REQUIRE(addr == base + i * 10240);
            REQUIRE(check_pattern(handles[i].ptr, 10000));
        }

        for (std::size_t i = 0; i < count; ++i)
            alloc.dealloc(handles[i].ptr);
    }

    SUBCASE("Cross-thread deallocation and abandon/adopt for medium pages")
    {
        void* from_worker = nullptr;
        std::thread worker([&]() {
            auto h = alloc.alloc(4096);
            from_worker = h.ptr;
            alloc.release_cache(mock_thread_routines::tcache);
        });
        worker.join();
        REQUIRE(from_worker != nullptr);

        // Adopt the abandoned medium page on the main thread.
        auto mine = alloc.alloc(4096);
        REQUIRE(mine.ptr != nullptr);
        REQUIRE(page_of(mine.ptr) == page_of(from_worker));
        REQUIRE(mine.ptr != from_worker);

        // Remote free from another thread onto our owned medium page.
        std::thread helper([&]() { alloc.dealloc(mine.ptr); });
        helper.join();

        alloc.dealloc(from_worker);
    }

    SUBCASE(
        "Non-head empty medium pages return to arena while head stays cached"
    )
    {
        mock_thread_routines::reset_tcache();
        counting_heap_allocator call_alloc;
        counting_mutex::lock_calls = 0;

        // 32768 B blocks: 2-slice span (131072 B) holds 4 blocks per page.
        std::vector<void*> page_a;
        for (int i = 0; i < 4; ++i)
            page_a.push_back(call_alloc.alloc(32768).ptr);

        // 5th block allocates page_b and places it at the head of the queue.
        void* page_b_blk = call_alloc.alloc(32768).ptr;
        REQUIRE(page_b_blk != nullptr);

        const std::size_t before_free = counting_mutex::lock_calls;
        // Freeing all blocks of non-head page_a releases its span to the arena.
        for (void* p : page_a)
            call_alloc.dealloc(p);
        REQUIRE(counting_mutex::lock_calls > before_free);

        // Freeing the sole block on head page_b keeps page_b cached (no lock),
        // and the next allocation reuses it without locking.
        const std::size_t after_release = counting_mutex::lock_calls;
        call_alloc.dealloc(page_b_blk);
        auto reused = call_alloc.alloc(32768);
        REQUIRE(reused.ptr != nullptr);
        call_alloc.dealloc(reused.ptr);
        REQUIRE(counting_mutex::lock_calls == after_release);
    }
}

TEST_CASE("heap_allocator alignment guarantees")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Valid alignments up to MaxSmallSize")
    {
        const std::size_t alignments[]
            = { 8, 16, 32, 64, 128, 256, 512, 1024, 2048 };

        for (std::size_t align : alignments) {
            auto handle = alloc.alloc(48, align);
            REQUIRE(handle.ptr != nullptr);
            REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % align == 0);
            alloc.dealloc(handle.ptr);
        }
    }

    SUBCASE("Alignments above MaxSmallSize up to PageSize")
    {
        const std::size_t alignments[] = { 4096, 16384, TEST_PAGE_SIZE };

        for (std::size_t align : alignments) {
            auto handle = alloc.alloc(100, align);
            REQUIRE(handle.ptr != nullptr);
            REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % align == 0);
            alloc.dealloc(handle.ptr);
        }

        // Large path with PageSize alignment.
        auto large = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 100, TEST_PAGE_SIZE);
        REQUIRE(large.ptr != nullptr);
        REQUIRE(
            reinterpret_cast<std::uintptr_t>(large.ptr) % TEST_PAGE_SIZE == 0
        );
        alloc.dealloc(large.ptr);
    }

    SUBCASE("Alignment above PageSize is rejected")
    {
        auto handle = alloc.alloc(64, TEST_PAGE_SIZE * 2);
        REQUIRE(handle.ptr == nullptr);
        REQUIRE(handle.count == 0);
    }

    SUBCASE("Invalid non-power-of-two alignment returns null handle")
    {
        auto handle = alloc.alloc(64, 15);
        REQUIRE(handle.ptr == nullptr);
        REQUIRE(handle.count == 0);
    }
}

TEST_CASE("heap_allocator cross-thread deallocation")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    // 128-byte blocks: exactly 512 per page, so a fresh page is fully used.
    constexpr std::size_t per_page = TEST_PAGE_SIZE / 128;
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < per_page; ++i) {
        auto h = alloc.alloc(128);
        REQUIRE(h.ptr != nullptr);
        blocks.push_back(h.ptr);
    }
    const std::uintptr_t page0 = page_of(blocks[0]);
    for (void* b : blocks)
        REQUIRE(page_of(b) == page0);

    SUBCASE("A remotely freed block is reclaimed by the owner")
    {
        // Another thread has a different thread_cache, so this is a remote
        // free onto the page's xthread_free list.
        std::thread worker([&]() { alloc.dealloc(blocks[0]); });
        worker.join();

        // The page's free list is empty, so the owner's slow path collects.
        auto stolen = alloc.alloc(128);
        REQUIRE(stolen.ptr == blocks[0]);

        alloc.dealloc(stolen.ptr);
        for (std::size_t i = 1; i < blocks.size(); ++i)
            alloc.dealloc(blocks[i]);
    }

    SUBCASE("Bulk remote frees are all recovered exactly once")
    {
        std::thread worker([&]() {
            for (std::size_t i = 1; i < blocks.size(); ++i)
                alloc.dealloc(blocks[i]);
        });
        worker.join();

        bool seen[per_page] = { };
        seen[0] = true; // blocks[0] is still allocated
        for (std::size_t i = 1; i < per_page; ++i) {
            auto h = alloc.alloc(128);
            REQUIRE(h.ptr != nullptr);
            REQUIRE(page_of(h.ptr) == page0);
            std::size_t idx
                = (reinterpret_cast<std::uintptr_t>(h.ptr) - page0) / 128;
            REQUIRE(idx < per_page);
            REQUIRE_FALSE(seen[idx]);
            seen[idx] = true;
        }
        alloc.dealloc(blocks[0]);
    }
}

TEST_CASE("heap_allocator abandon and adopt pages across threads")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    void* from_worker = nullptr;
    std::thread worker([&]() {
        auto h = alloc.alloc(64);
        from_worker = h.ptr;
        // Thread/CPU going away: block still live, so the page is abandoned.
        alloc.release_cache(mock_thread_routines::tcache);
    });
    worker.join();
    REQUIRE(from_worker != nullptr);

    // Main thread has no pages for this bucket; it adopts the abandoned page.
    auto mine = alloc.alloc(64);
    REQUIRE(mine.ptr != nullptr);
    REQUIRE(page_of(mine.ptr) == page_of(from_worker));
    REQUIRE(mine.ptr != from_worker);

    // We own the adopted page now, so freeing the worker's block is local.
    alloc.dealloc(from_worker);
    alloc.dealloc(mine.ptr);
}

TEST_CASE("heap_allocator arenas grow on demand")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    // 2048-byte blocks: 32 per page, 15 usable pages per arena => 480 blocks
    // per arena. 1000 blocks need at least three arenas.
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < 1000; ++i) {
        auto h = alloc.alloc(2048);
        REQUIRE(h.ptr != nullptr);
        blocks.push_back(h.ptr);
    }
    for (void* b : blocks)
        alloc.dealloc(b);

    // Empty pages are returned to their arenas and can be reused.
    blocks.clear();
    for (std::size_t i = 0; i < 1000; ++i) {
        auto h = alloc.alloc(2048);
        REQUIRE(h.ptr != nullptr);
        blocks.push_back(h.ptr);
    }
    for (void* b : blocks)
        alloc.dealloc(b);
}

TEST_CASE("heap_allocator large allocations come from arena slices")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Allocation exceeding MaxMediumSize")
    {
        std::size_t largeSize = TEST_MAX_MEDIUM_SIZE + 1024;
        auto handle = alloc.alloc(largeSize);

        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= largeSize);
        // No header in the page: payload starts on a slice boundary.
        REQUIRE(
            reinterpret_cast<std::uintptr_t>(handle.ptr) % TEST_PAGE_SIZE == 0
        );
        REQUIRE(alloc.usable_size(handle.ptr) == handle.count);

        alloc.dealloc(handle.ptr);
    }

    SUBCASE("Multi-slice allocation is fully usable")
    {
        std::size_t n = TEST_PAGE_SIZE * 3;
        auto handle = alloc.alloc(n);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= n);
        fill_pattern(handle.ptr, n);
        REQUIRE(check_pattern(handle.ptr, n));
        alloc.dealloc(handle.ptr);
    }

    SUBCASE("Freed large span is reused")
    {
        auto a = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024);
        alloc.dealloc(a.ptr);
        auto b = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024);
        REQUIRE(b.ptr == a.ptr);
        alloc.dealloc(b.ptr);
    }
}

TEST_CASE("heap_allocator huge allocations get a dedicated arena")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    auto handle = alloc.alloc(TEST_HUGE_SIZE);
    REQUIRE(handle.ptr != nullptr);
    REQUIRE(handle.count >= TEST_HUGE_SIZE);
    REQUIRE(alloc.usable_size(handle.ptr) == handle.count);
    REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % 64 == 0);

    fill_pattern(handle.ptr, TEST_HUGE_SIZE);
    REQUIRE(check_pattern(handle.ptr, TEST_HUGE_SIZE));

    // ptr_page() must resolve a huge block too: dealloc exercises it.
    alloc.dealloc(handle.ptr);

    auto aligned = alloc.alloc(TEST_HUGE_SIZE, 4096);
    REQUIRE(aligned.ptr != nullptr);
    REQUIRE(reinterpret_cast<std::uintptr_t>(aligned.ptr) % 4096 == 0);
    alloc.dealloc(aligned.ptr);
}

TEST_CASE(
    "heap_allocator copes with a page allocator that is not arena-aligned"
)
{
    mock_thread_routines::reset_tcache();
    offset_heap_allocator alloc;

    auto small = alloc.alloc(64);
    REQUIRE(small.ptr != nullptr);

    auto medium = alloc.alloc(8192);
    REQUIRE(medium.ptr != nullptr);

    auto large = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024);
    REQUIRE(large.ptr != nullptr);

    auto huge = alloc.alloc(TEST_HUGE_SIZE);
    REQUIRE(huge.ptr != nullptr);
    fill_pattern(huge.ptr, TEST_HUGE_SIZE);
    REQUIRE(check_pattern(huge.ptr, TEST_HUGE_SIZE));

    alloc.dealloc(huge.ptr);
    alloc.dealloc(large.ptr);
    alloc.dealloc(medium.ptr);
    alloc.dealloc(small.ptr);
}

TEST_CASE("heap_allocator reports page allocator failure")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    mock_page_allocator::simulate_failure = true;
    REQUIRE(alloc.alloc(64).ptr == nullptr);
    REQUIRE(alloc.alloc(8192).ptr == nullptr);
    REQUIRE(alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024).ptr == nullptr);
    REQUIRE(alloc.alloc(TEST_HUGE_SIZE).ptr == nullptr);
    mock_page_allocator::simulate_failure = false;

    // The heap must still be usable after a failed attempt.
    auto h = alloc.alloc(64);
    REQUIRE(h.ptr != nullptr);
    alloc.dealloc(h.ptr);
}

TEST_CASE("heap_allocator grow and shrink API")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("grow(nullptr) acts as alloc")
    {
        auto handle = alloc.grow(nullptr, 64);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= 64);
        alloc.dealloc(handle.ptr);
    }

    SUBCASE("grow(ptr, 0) acts as dealloc")
    {
        auto handle = alloc.alloc(64);
        auto new_handle = alloc.grow(handle.ptr, 0);
        REQUIRE(new_handle.ptr == nullptr);
        REQUIRE(new_handle.count == 0);
    }

    SUBCASE("In-place growth within bucket capacity")
    {
        auto handle = alloc.alloc(20);
        REQUIRE(handle.count >= 24);

        auto grown = alloc.grow(handle.ptr, 24);
        REQUIRE(grown.ptr == handle.ptr);
        REQUIRE(grown.count == handle.count);

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Out-of-place growth moving to larger bucket keeps the data")
    {
        auto handle = alloc.alloc(32);
        fill_pattern(handle.ptr, 32);

        auto grown = alloc.grow(handle.ptr, 128);
        REQUIRE(grown.ptr != nullptr);
        REQUIRE(grown.ptr != handle.ptr);
        REQUIRE(grown.count >= 128);
        REQUIRE(check_pattern(grown.ptr, 32));

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Medium block growth within sub-bin and across sub-bins")
    {
        auto handle = alloc.alloc(3000); // 3072-byte sub-bin
        REQUIRE(handle.count == 3072);
        fill_pattern(handle.ptr, 3000);

        // Fits within 3072 B sub-bin: stays in place.
        auto in_bin = alloc.grow(handle.ptr, 3072);
        REQUIRE(in_bin.ptr == handle.ptr);
        REQUIRE(in_bin.count == 3072);

        // Exceeds 3072 B: moves to a larger medium sub-bin and preserves data.
        auto moved = alloc.grow(in_bin.ptr, 12000);
        REQUIRE(moved.ptr != nullptr);
        REQUIRE(moved.ptr != handle.ptr);
        REQUIRE(moved.count >= 12000);
        REQUIRE(check_pattern(moved.ptr, 3000));

        // Shrinking by less than a full slice keeps the medium block in place.
        auto shrunk = alloc.shrink(moved.ptr, 4096);
        REQUIRE(shrunk.ptr == moved.ptr);
        REQUIRE(check_pattern(shrunk.ptr, 3000));

        alloc.dealloc(shrunk.ptr);
    }

    SUBCASE("Shrinking a small block stays in place")
    {
        auto handle = alloc.alloc(128);
        auto shrunk = alloc.shrink(handle.ptr, 64);
        REQUIRE(shrunk.ptr == handle.ptr);
        alloc.dealloc(shrunk.ptr);
    }

    SUBCASE("Growing a large block that already fits is a no-op")
    {
        std::size_t large_size = TEST_MAX_MEDIUM_SIZE + 4096;
        auto handle = alloc.alloc(large_size);
        REQUIRE(handle.ptr != nullptr);

        auto grown = alloc.grow(handle.ptr, large_size + 8192);
        REQUIRE(grown.ptr == handle.ptr);
        REQUIRE(grown.count >= large_size + 8192);

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Large arena page grows and shrinks in place across slices")
    {
        auto handle = alloc.alloc(TEST_PAGE_SIZE * 2);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count == TEST_PAGE_SIZE * 2);
        fill_pattern(handle.ptr, TEST_PAGE_SIZE * 2);

        // The neighbouring slice is free in a fresh arena, so the page's
        // metadata stays put and only its span changes.
        auto grown = alloc.grow(handle.ptr, TEST_PAGE_SIZE * 3);
        REQUIRE(grown.ptr == handle.ptr);
        REQUIRE(grown.count == TEST_PAGE_SIZE * 3);
        REQUIRE(alloc.usable_size(grown.ptr) == TEST_PAGE_SIZE * 3);
        REQUIRE(check_pattern(grown.ptr, TEST_PAGE_SIZE * 2));

        auto shrunk = alloc.shrink(grown.ptr, TEST_PAGE_SIZE * 2);
        REQUIRE(shrunk.ptr == grown.ptr);
        REQUIRE(shrunk.count == TEST_PAGE_SIZE * 2);
        REQUIRE(alloc.usable_size(shrunk.ptr) == TEST_PAGE_SIZE * 2);

        alloc.dealloc(shrunk.ptr);
    }

    SUBCASE("Growing a huge block moves it and rebuilds metadata")
    {
        auto handle = alloc.alloc(TEST_HUGE_SIZE);
        REQUIRE(handle.ptr != nullptr);
        fill_pattern(handle.ptr, TEST_HUGE_SIZE);

        auto grown = alloc.grow(handle.ptr, TEST_HUGE_SIZE * 2);
        REQUIRE(grown.ptr != nullptr);
        REQUIRE(grown.count >= TEST_HUGE_SIZE * 2);
        REQUIRE(alloc.usable_size(grown.ptr) == grown.count);
        REQUIRE(check_pattern(grown.ptr, TEST_HUGE_SIZE));

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Shrinking a huge block returns memory and keeps the data")
    {
        auto handle = alloc.alloc(TEST_HUGE_SIZE * 2);
        REQUIRE(handle.ptr != nullptr);
        fill_pattern(handle.ptr, TEST_HUGE_SIZE);

        auto shrunk = alloc.shrink(handle.ptr, TEST_HUGE_SIZE);
        REQUIRE(shrunk.ptr != nullptr);
        REQUIRE(shrunk.count >= TEST_HUGE_SIZE);
        REQUIRE(check_pattern(shrunk.ptr, TEST_HUGE_SIZE));

        alloc.dealloc(shrunk.ptr);
    }
}

TEST_CASE("heap_allocator inplace_grow and inplace_shrink API")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("inplace_grow succeeds within capacity")
    {
        auto handle = alloc.alloc(20); // 24-byte bucket
        void* ptr = alloc.grow_inplace(handle.ptr, 24);
        REQUIRE(ptr == handle.ptr);
        alloc.dealloc(handle.ptr);
    }

    SUBCASE("inplace_grow fails when exceeding capacity")
    {
        auto handle = alloc.alloc(32);
        void* ptr = alloc.grow_inplace(handle.ptr, 64);
        REQUIRE(ptr == nullptr);
        alloc.dealloc(handle.ptr);
    }

    SUBCASE("inplace_shrink always succeeds within capacity")
    {
        auto handle = alloc.alloc(64);
        void* ptr = alloc.shrink_inplace(handle.ptr, 32);
        REQUIRE(ptr == handle.ptr);
        alloc.dealloc(handle.ptr);
    }
}

TEST_CASE("heap_allocator atomic allocation and deallocation (interrupt-like)")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("alloc_atomic never falls back to the regular allocator")
    {
        auto handle = alloc.alloc_atomic(64);
        REQUIRE(handle.ptr == nullptr);
        REQUIRE(handle.count == 0);

        // Large requests are not served from atomic context at all.
        REQUIRE(alloc.alloc_atomic(TEST_MAX_MEDIUM_SIZE + 1024).ptr == nullptr);
    }

    SUBCASE("alloc_atomic serves only blocks staged by reserve_atomic")
    {
        REQUIRE(alloc.reserve_atomic(64, 4) == 4);

        void* got[4] = { };
        for (auto& g : got) {
            auto h = alloc.alloc_atomic(64);
            REQUIRE(h.ptr != nullptr);
            REQUIRE(h.count >= 64);
            REQUIRE(
                reinterpret_cast<std::uintptr_t>(h.ptr) % TEST_GRANULARITY == 0
            );
            g = h.ptr;
        }
        for (std::size_t i = 0; i < 4; ++i)
            for (std::size_t j = i + 1; j < 4; ++j)
                REQUIRE(got[i] != got[j]);

        // Stash is empty again.
        REQUIRE(alloc.alloc_atomic(64).ptr == nullptr);

        for (void* g : got)
            alloc.dealloc_atomic(g);
    }

    SUBCASE("Medium blocks can be staged and freed atomically")
    {
        REQUIRE(alloc.reserve_atomic(4096, 3) == 3);

        void* got[3] = { };
        for (auto& g : got) {
            auto h = alloc.alloc_atomic(4096);
            REQUIRE(h.ptr != nullptr);
            REQUIRE(h.count == 4096);
            g = h.ptr;
        }
        REQUIRE(alloc.alloc_atomic(4096).ptr == nullptr);

        for (void* g : got)
            alloc.dealloc_atomic(g);
    }

    SUBCASE("Atomically freed blocks are accounted for and come back")
    {
        // 64-byte blocks: 1024 per page.
        constexpr std::size_t per_page = TEST_PAGE_SIZE / 64;
        REQUIRE(alloc.reserve_atomic(64, 4) == 4);

        void* staged[4] = { };
        for (auto& s : staged)
            s = alloc.alloc_atomic(64).ptr;
        for (void* s : staged) {
            REQUIRE(s != nullptr);
            alloc.dealloc_atomic(s); // goes onto the page's xthread_free list
        }

        // All 1024 blocks of the page are free again, four of them only via
        // the atomic path. The owner's slow path folds those in whenever the
        // free list runs dry (every few blocks while the page is carved
        // lazily), so we cannot predict which allocation returns them.
        // Instead: a full page's worth of allocations must hand out every
        // block exactly once and never spill onto a second page. That only
        // works if the atomic frees were collected and counted correctly.
        const std::uintptr_t page0 = page_of(staged[0]);
        bool seen[per_page] = { };
        std::vector<void*> all;
        for (std::size_t i = 0; i < per_page; ++i) {
            auto h = alloc.alloc(64);
            REQUIRE(h.ptr != nullptr);
            REQUIRE(page_of(h.ptr) == page0);
            std::size_t idx
                = (reinterpret_cast<std::uintptr_t>(h.ptr) - page0) / 64;
            REQUIRE(idx < per_page);
            REQUIRE_FALSE(seen[idx]);
            seen[idx] = true;
            all.push_back(h.ptr);
        }
        for (void* s : staged) {
            std::size_t idx
                = (reinterpret_cast<std::uintptr_t>(s) - page0) / 64;
            REQUIRE(seen[idx]);
        }

        for (void* p : all)
            alloc.dealloc(p);
    }

    SUBCASE("dealloc_atomic of a large block is deferred, then released")
    {
        auto h = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024);
        REQUIRE(h.ptr != nullptr);
        alloc.dealloc_atomic(h.ptr);

        // The next normal-context large allocation drains the deferred list and
        // reuses the span.
        auto h2 = alloc.alloc(TEST_MAX_MEDIUM_SIZE + 1024);
        REQUIRE(h2.ptr == h.ptr);
        alloc.dealloc(h2.ptr);
    }
}

TEST_CASE("heap_allocator custom mutex type")
{
    mock_thread_routines::reset_tcache();
    counting_heap_allocator alloc;
    counting_mutex::lock_calls = 0;

    // First allocation needs an arena and a page: slow path, takes locks.
    auto first = alloc.alloc(64);
    REQUIRE(first.ptr != nullptr);
    REQUIRE(counting_mutex::lock_calls > 0);

    // Further small and medium allocations and frees on the same pages never
    // lock.
    const std::size_t baseline = counting_mutex::lock_calls;
    std::vector<void*> blocks;
    for (std::size_t i = 0; i < 100; ++i)
        blocks.push_back(alloc.alloc(64).ptr);
    for (void* b : blocks) {
        REQUIRE(b != nullptr);
        alloc.dealloc(b);
    }
    alloc.dealloc(first.ptr);

    auto first_med = alloc.alloc(4096);
    REQUIRE(first_med.ptr != nullptr);
    const std::size_t med_baseline = counting_mutex::lock_calls;
    std::vector<void*> med_blocks;
    for (int i = 0; i < 15; ++i)
        med_blocks.push_back(alloc.alloc(4096).ptr);
    for (void* b : med_blocks) {
        REQUIRE(b != nullptr);
        alloc.dealloc(b);
    }
    alloc.dealloc(first_med.ptr);
    REQUIRE(counting_mutex::lock_calls == med_baseline);
}
