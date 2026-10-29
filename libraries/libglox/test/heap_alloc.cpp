#ifndef USE_MODULES
#include "glox/heap_alloc.hpp"
#include "glox/alloc_types.hpp"
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

constexpr std::size_t TestPageSize = 65536;
constexpr std::size_t TestMaxSmallSize = 2048;
constexpr std::size_t TestGranularity = 8;
constexpr std::size_t TestNumBuckets = TestMaxSmallSize / TestGranularity;

// 1. Mock Page Allocator delivering 64KB page-aligned memory
struct mock_page_allocator
{
    bool simulate_failure = false;

    glox::alloc_handle<void> alloc(std::size_t n)
    {
        if (simulate_failure) {
            return { nullptr, 0 };
        }

        void* ptr = nullptr;
        if (posix_memalign(&ptr, TestPageSize, n) != 0) {
            return { nullptr, 0 };
        }
        return { ptr, n };
    }

    void dealloc(void* ptr, std::size_t n)
    {
        (void)n;
        if (!ptr)
            return;
        free(ptr);
    }

    glox::alloc_handle<void>
    grow(void* ptr, std::size_t old_n, std::size_t new_n)
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

    glox::alloc_handle<void>
    shrink(void* ptr, std::size_t old_n, std::size_t new_n)
    {
        return grow(ptr, old_n, new_n);
    }
};

// 2. Mock Thread Routines with configurable thread IDs and per-thread caches
struct mock_thread_routines
{
    static inline thread_local std::size_t current_tid = 1;
    static inline thread_local glox::thread_cache<TestNumBuckets> tcache { };

    std::size_t thread_id() const noexcept
    {
        return current_tid;
    }

    glox::thread_cache<TestNumBuckets>& get_tcache() const noexcept
    {
        return tcache;
    }

    static void reset_tcache()
    {
        tcache = glox::thread_cache<TestNumBuckets> { };
    }
};

using test_heap_allocator = glox::heap_allocator<
    mock_page_allocator,
    mock_thread_routines,
    TestMaxSmallSize,
    TestGranularity,
    TestPageSize
>;

} // namespace

TEST_CASE("heap_allocator basic small allocation & deallocation")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Single allocation and size-less deallocation fast-path")
    {
        auto handle = alloc.alloc(32);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= 32);
        REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % 8 == 0);

        // Deallocate back to local freelist (using new size-less API)
        alloc.dealloc(handle.ptr);

        // Next allocation of same size should hit local freelist and reuse
        // pointer
        auto handle2 = alloc.alloc(32);
        REQUIRE(handle2.ptr == handle.ptr);

        alloc.dealloc(handle2.ptr);
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

        // Verify all returned pointers are unique
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = i + 1; j < count; ++j) {
                REQUIRE(handles[i].ptr != handles[j].ptr);
            }
        }

        for (std::size_t i = 0; i < count; ++i) {
            alloc.dealloc(handles[i].ptr);
        }
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

    SUBCASE("Invalid non-power-of-two alignment returns null handle")
    {
        auto handle = alloc.alloc(64, 15);
        REQUIRE(handle.ptr == nullptr);
        REQUIRE(handle.count == 0);
    }
}

TEST_CASE("heap_allocator cross-thread lock-free deallocation & stealing")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    mock_thread_routines::current_tid = 1;
    auto handle = alloc.alloc(128);
    REQUIRE(handle.ptr != nullptr);

    // Cross-thread free from Thread 2
    std::thread worker([&]() {
        mock_thread_routines::current_tid = 2;
        alloc.dealloc(handle.ptr);
    });
    worker.join();

    // Back on Thread 1: Drain local freelist first by allocating blocks
    mock_thread_routines::current_tid = 1;

    // Next slow-path allocation on Thread 1 should steal the cross-thread freed
    // block
    auto stolenHandle = alloc.alloc(128);
    REQUIRE(stolenHandle.ptr != nullptr);

    alloc.dealloc(stolenHandle.ptr);
}

TEST_CASE("heap_allocator large allocations fallback to page_allocator")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Allocation exceeding MaxSmallSize")
    {
        std::size_t largeSize = TestMaxSmallSize + 1024;
        auto handle = alloc.alloc(largeSize);

        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= largeSize);

        // Large payloads are offset by the header, so they align to
        // Granularity/requested align, not strictly TestPageSize anymore.
        REQUIRE(
            reinterpret_cast<std::uintptr_t>(handle.ptr) % TestGranularity == 0
        );

        alloc.dealloc(handle.ptr);
    }
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
        // Allocate 20 bytes (fits in 24 byte bucket if Granularity=8)
        auto handle = alloc.alloc(20);
        REQUIRE(handle.count >= 24);

        // Grow to 24 bytes (should succeed in-place)
        auto grown = alloc.grow(handle.ptr, 24);
        REQUIRE(grown.ptr == handle.ptr);
        REQUIRE(grown.count == handle.count);

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Out-of-place growth moving to larger bucket")
    {
        auto handle = alloc.alloc(32);

        // Grow to 128 (forces a new allocation and memcpy)
        auto grown = alloc.grow(handle.ptr, 128);
        REQUIRE(grown.ptr != nullptr);
        REQUIRE(grown.ptr != handle.ptr); // Pointer must change
        REQUIRE(grown.count >= 128);

        alloc.dealloc(grown.ptr);
    }

    SUBCASE("Shrinking a small block")
    {
        auto handle = alloc.alloc(128);

        // Shrinking small blocks remains in-place because moving them isn't
        // worth the overhead
        auto shrunk = alloc.shrink(handle.ptr, 64);
        REQUIRE(shrunk.ptr == handle.ptr);

        alloc.dealloc(shrunk.ptr);
    }

    SUBCASE("Growing a large block")
    {
        std::size_t large_size = TestMaxSmallSize + 4096; // ~6KB
        auto handle = alloc.alloc(large_size);
        REQUIRE(handle.ptr != nullptr);

        std::size_t huge_size = large_size * 2;
        auto grown = alloc.grow(handle.ptr, huge_size);
        REQUIRE(grown.ptr != nullptr);
        REQUIRE(grown.count >= huge_size);

        alloc.dealloc(grown.ptr);
    }
}

TEST_CASE("heap_allocator inplace_grow and inplace_shrink API")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("inplace_grow succeeds within capacity")
    {
        auto handle = alloc.alloc(20); // Goes to 24-byte bucket
        void* ptr = alloc.grow_inplace(handle.ptr, 24);
        REQUIRE(ptr == handle.ptr);

        alloc.dealloc(handle.ptr);
    }

    SUBCASE("inplace_grow fails when exceeding capacity")
    {
        auto handle = alloc.alloc(32);
        void* ptr = alloc.grow_inplace(handle.ptr, 64);

        // Cannot grow in place, must return nullptr
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
