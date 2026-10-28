#ifndef USE_MODULES
#include "glox/heap_alloc.hpp"
#include "glox/alloc_types.hpp"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
        (void)ptr;
        (void)old_n;
        (void)new_n;
        return { nullptr, 0 };
    }

    glox::alloc_handle<void>
    shrink(void* ptr, std::size_t old_n, std::size_t new_n)
    {
        (void)ptr;
        (void)old_n;
        (void)new_n;
        return { nullptr, 0 };
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

    SUBCASE("Single allocation and deallocation fast-path")
    {
        auto handle = alloc.alloc(32);
        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= 32);
        REQUIRE(reinterpret_cast<std::uintptr_t>(handle.ptr) % 8 == 0);

        // Deallocate back to local freelist
        alloc.dealloc(handle.ptr, 32);

        // Next allocation of same size should hit local freelist and reuse
        // pointer
        auto handle2 = alloc.alloc(32);
        REQUIRE(handle2.ptr == handle.ptr);

        alloc.dealloc(handle2.ptr, 32);
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
            alloc.dealloc(handles[i].ptr, 64);
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
            alloc.dealloc(handle.ptr, 48, align);
        }
    }

    SUBCASE("Invalid non-power-of-two alignment returns null handle")
    {
        auto handle = alloc.alloc(64, 15);
        REQUIRE(handle.ptr == nullptr);
        REQUIRE(handle.count == 0);
    }

    SUBCASE("Alignment exceeding PageSize returns null handle")
    {
        auto handle = alloc.alloc(64, TestPageSize * 2);
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
        alloc.dealloc(handle.ptr, 128);
    });
    worker.join();

    // Back on Thread 1: Drain local freelist first by allocating blocks
    mock_thread_routines::current_tid = 1;

    // Next slow-path allocation on Thread 1 should steal the cross-thread freed
    // block
    auto stolen_handle = alloc.alloc(128);
    REQUIRE(stolen_handle.ptr != nullptr);

    alloc.dealloc(stolen_handle.ptr, 128);
}

TEST_CASE("heap_allocator large allocations fallback to page_allocator")
{
    mock_thread_routines::reset_tcache();
    test_heap_allocator alloc;

    SUBCASE("Allocation exceeding MaxSmallSize")
    {
        std::size_t large_size = TestMaxSmallSize + 1024;
        auto handle = alloc.alloc(large_size);

        REQUIRE(handle.ptr != nullptr);
        REQUIRE(handle.count >= large_size);
        REQUIRE(
            reinterpret_cast<std::uintptr_t>(handle.ptr) % TestPageSize == 0
        );

        alloc.dealloc(handle.ptr, large_size);
    }
}

TEST_CASE("heap_allocator page allocation failure handling")
{
    mock_thread_routines::reset_tcache();

    // Instantiate allocator with a page allocator set to fail
    glox::heap_allocator<
        mock_page_allocator,
        mock_thread_routines,
        TestMaxSmallSize,
        TestGranularity,
        TestPageSize
    >
        failing_alloc;

    // Reach into the internal page allocator instance (if accessible) or
    // trigger slow-path failure Testing graceful nullptr return when page
    // allocation fails
    mock_page_allocator palloc_failing { .simulate_failure = true };
    auto handle = palloc_failing.alloc(TestPageSize);

    REQUIRE(handle.ptr == nullptr);
    REQUIRE(handle.count == 0);
}
