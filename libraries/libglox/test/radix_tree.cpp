#ifndef USE_MODULES
#include "glox/radix_tree.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>
#else
import std;
import glox;
import std.compat;
#endif
#include "doctest.h"
#include "glox/assert.hpp"

using namespace glox;

namespace {

// Tagged PFN-style descriptor to exercise realistic kernel usage
struct test_pfn_desc
{
    uint64_t bits = 0;

    constexpr test_pfn_desc() noexcept = default;
    constexpr explicit test_pfn_desc(uint64_t b) noexcept
        : bits { b }
    {
    }

    static constexpr test_pfn_desc make_cache(uintptr_t ptr) noexcept
    {
        return test_pfn_desc { static_cast<uint64_t>(ptr) | 1ULL };
    }

    static constexpr test_pfn_desc make_hw(uint64_t refcount) noexcept
    {
        return test_pfn_desc { (refcount << 3) | 3ULL };
    }

    constexpr explicit operator bool() const noexcept
    {
        return bits != 0;
    }

    friend constexpr bool operator==(test_pfn_desc, test_pfn_desc) noexcept
        = default;
};

// Stateful tracking allocator that satisfies
// glox::allocator<tracking_allocator<T>, T> and verifies every allocated radix
// node is properly freed.
struct alloc_stats
{
    size_t live_nodes = 0;
    size_t total_allocs = 0;
    size_t total_deallocs = 0;
    size_t fail_after = -1;
};

template <typename T>
struct tracking_allocator
{
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind
    {
        using other = tracking_allocator<U>;
    };

    alloc_stats* stats = nullptr;

    constexpr tracking_allocator() noexcept = default;
    explicit constexpr tracking_allocator(alloc_stats* s) noexcept
        : stats { s }
    {
    }

    template <typename U>
    constexpr tracking_allocator(tracking_allocator<U> const& other) noexcept
        : stats { other.stats }
    {
    }

    alloc_handle<T> alloc(std::size_t count, std::size_t alignment) noexcept
    {
        if (stats) {
            if (stats->total_allocs >= stats->fail_after) {
                return { nullptr, 0 };
            }
            stats->total_allocs += 1;
            stats->live_nodes += 1;
        }
        void* p = std::aligned_alloc(alignment, count * sizeof(T));
        if (!p) {
            if (stats) {
                stats->live_nodes -= 1;
            }
            return { nullptr, 0 };
        }
        return { static_cast<T*>(p), count };
    }

    alloc_handle<T> alloc_zeroed(
        std::size_t count,
        std::size_t alignment
    ) noexcept
    {
        auto mem = alloc(count, alignment);
        if (mem.ptr != nullptr)
            memset(mem.ptr, 0, sizeof(T) * count);

        return mem;
    }

    void dealloc(
        T* p,
        [[maybe_unused]] std::size_t count,
        [[maybe_unused]] std::size_t alignment
    ) noexcept
    {
        if (!p) {
            return;
        }
        if (stats) {
            GLOX_ASSERT(stats->live_nodes > 0);
            stats->live_nodes -= 1;
            stats->total_deallocs += 1;
        }
        std::free(static_cast<void*>(p));
    }
};

using tracked_tree = radix_tree<uint64_t, tracking_allocator<void>>;

std::vector<uint64_t> shuffled_keys(uint64_t count, unsigned seed)
{
    std::vector<uint64_t> keys(count);
    for (uint64_t i = 0; i < count; ++i) {
        keys[i] = i;
    }
    std::shuffle(keys.begin(), keys.end(), std::mt19937 { seed });
    return keys;
}

} // namespace

TEST_CASE("radix_tree: empty state")
{
    alloc_stats stats;
    tracked_tree tree { tracking_allocator<void> { &stats } };

    CHECK(tree.is_empty());
    CHECK_EQ(stats.live_nodes, 0u);
    CHECK(tree.find(0) == nullptr);
    CHECK(tree.find(42) == nullptr);
    CHECK(tree.find(~uint64_t { 0 }) == nullptr);
    CHECK_FALSE(tree.erase(0));
    CHECK_FALSE(tree.erase(123456));
}

TEST_CASE("radix_tree: single insert, lookup and erase")
{
    alloc_stats stats;
    {
        tracked_tree tree { tracking_allocator<void> { &stats } };

        CHECK(tree.insert(0, 0xdeadbeefULL));
        CHECK_FALSE(tree.is_empty());
        CHECK_GT(stats.live_nodes, 0u);

        auto* slot = tree.find(0);
        REQUIRE(slot != nullptr);
        CHECK_EQ(slot->load(std::memory_order_relaxed), 0xdeadbeefULL);
        CHECK(tree.find(1) == nullptr);

        // Overwrite existing key
        CHECK(tree.insert(0, 0xcafebabeULL));
        CHECK_EQ(tree.find(0)->load(std::memory_order_relaxed), 0xcafebabeULL);

        // Erase prunes back down to zero live nodes
        CHECK(tree.erase(0));
        CHECK_FALSE(tree.erase(0));
        CHECK(tree.is_empty());
        CHECK(tree.find(0) == nullptr);
        CHECK_EQ(stats.live_nodes, 0u);
    }
    CHECK_EQ(stats.total_allocs, stats.total_deallocs);
}

TEST_CASE("radix_tree: find_or_insert distinguishes new vs existing")
{
    radix_tree<uint64_t> tree;

    auto first = tree.find_or_insert(1024);
    REQUIRE(first.slot != nullptr);
    CHECK(first.inserted);
    first.slot->store(77, std::memory_order_relaxed);

    auto second = tree.find_or_insert(1024);
    REQUIRE(second.slot != nullptr);
    CHECK_FALSE(second.inserted);
    CHECK_EQ(second.slot, first.slot);
    CHECK_EQ(second.slot->load(std::memory_order_relaxed), 77u);
}

TEST_CASE("radix_tree: dynamic upward height growth and sparse 64-bit keys")
{
    alloc_stats stats;
    {
        tracked_tree tree { tracking_allocator<void> { &stats } };

        // Start at key 0 (height 0), then jump progressively across 64-bit
        // boundaries
        uint64_t const keys[] = { 0ULL,
            1ULL,
            63ULL,
            64ULL,
            4096ULL,
            (1ULL << 20) + 7,
            (1ULL << 36) + 12345,
            (1ULL << 52) + 999,
            (1ULL << 63),
            ~uint64_t { 0 } - 1,
            ~uint64_t { 0 } };

        for (size_t i = 0; i < std::size(keys); ++i) {
            CHECK(tree.insert(keys[i], i + 100));
        }

        // Verify every inserted key and check neighboring absent keys
        for (size_t i = 0; i < std::size(keys); ++i) {
            auto* s = tree.find(keys[i]);
            REQUIRE(s != nullptr);
            CHECK_EQ(s->load(std::memory_order_relaxed), i + 100);
        }

        CHECK(tree.find(2ULL) == nullptr);
        CHECK(tree.find(65ULL) == nullptr);
        CHECK(tree.find((1ULL << 36) + 12346) == nullptr);
        CHECK(tree.find(~uint64_t { 0 } - 2) == nullptr);

        // Erase in reverse order and confirm full pruning
        for (size_t i = std::size(keys); i > 0; --i) {
            CHECK(tree.erase(keys[i - 1]));
        }
        CHECK(tree.is_empty());
        CHECK_EQ(stats.live_nodes, 0u);
    }
    CHECK_EQ(stats.total_allocs, stats.total_deallocs);
}

TEST_CASE("radix_tree: dense insert and shuffled erase reclaims all nodes")
{
    alloc_stats stats;
    {
        tracked_tree tree { tracking_allocator<void> { &stats } };
        constexpr uint64_t count = 5000;

        for (uint64_t k : shuffled_keys(count, 1)) {
            CHECK(tree.insert(k, k * 10 + 1));
        }

        bool allFound = true;
        for (uint64_t k = 0; k < count; ++k) {
            auto* slot = tree.find(k);
            allFound &= (slot != nullptr)
                     && (slot->load(std::memory_order_relaxed) == k * 10 + 1);
        }
        CHECK(allFound);

        for (uint64_t k : shuffled_keys(count, 2)) {
            CHECK(tree.erase(k));
        }
        CHECK(tree.is_empty());
        CHECK_EQ(stats.live_nodes, 0u);
    }
    CHECK_EQ(stats.total_allocs, stats.total_deallocs);
}

TEST_CASE("radix_tree: clear and reuse")
{
    alloc_stats stats;
    tracked_tree tree { tracking_allocator<void> { &stats } };

    for (uint64_t i = 0; i < 1000; ++i) {
        CHECK(tree.insert(i * 17, i + 1));
    }
    CHECK_FALSE(tree.is_empty());
    CHECK_GT(stats.live_nodes, 0u);

    tree.clear();
    CHECK(tree.is_empty());
    CHECK_EQ(stats.live_nodes, 0u);
    CHECK(tree.find(0) == nullptr);
    CHECK(tree.find(17 * 500) == nullptr);

    // Tree remains usable after clear()
    CHECK(tree.insert(999999ULL, 42ULL));
    REQUIRE(tree.find(999999ULL) != nullptr);
    CHECK_EQ(tree.find(999999ULL)->load(std::memory_order_relaxed), 42ULL);

    tree.clear();
    CHECK_EQ(stats.live_nodes, 0u);
}

TEST_CASE("radix_tree: tagged PfnDescriptor values")
{
    radix_tree<test_pfn_desc> pfn_tree;
    alignas(8) int dummy_cache_page = 0;

    uint64_t const pfn1 = 0x1000;
    uint64_t const pfn2 = 0x100000;

    CHECK(pfn_tree.insert(
        pfn1,
        test_pfn_desc::make_cache(
            reinterpret_cast<uintptr_t>(&dummy_cache_page)
        )
    ));
    CHECK(pfn_tree.insert(pfn2, test_pfn_desc::make_hw(5)));

    auto* s1 = pfn_tree.find(pfn1);
    REQUIRE(s1 != nullptr);
    CHECK_EQ(
        s1->load(std::memory_order_relaxed).bits,
        reinterpret_cast<uintptr_t>(&dummy_cache_page) | 1ULL
    );

    // Atomic in-place update without touching the writer lock
    auto* s2 = pfn_tree.find(pfn2);
    REQUIRE(s2 != nullptr);
    s2->store(test_pfn_desc::make_hw(6), std::memory_order_relaxed);
    CHECK_EQ(
        pfn_tree.find(pfn2)->load(std::memory_order_relaxed).bits,
        (6ULL << 3) | 3ULL
    );
}

TEST_CASE("radix_tree: allocation failure leaves tree consistent")
{
    alloc_stats stats;
    tracked_tree tree { tracking_allocator<void> { &stats } };

    // Populate initial subtree
    CHECK(tree.insert(1, 11));
    CHECK(tree.insert(2, 22));

    // Force next node allocation (for a distant key requiring new height/nodes)
    // to fail
    stats.fail_after = stats.total_allocs;
    CHECK_FALSE(tree.insert(1ULL << 48, 99));

    // Existing keys must remain intact and tree must free cleanly
    REQUIRE(tree.find(1) != nullptr);
    CHECK_EQ(tree.find(1)->load(std::memory_order_relaxed), 11u);
    REQUIRE(tree.find(2) != nullptr);
    CHECK_EQ(tree.find(2)->load(std::memory_order_relaxed), 22u);
    CHECK(tree.find(1ULL << 48) == nullptr);

    tree.clear();
    CHECK_EQ(stats.live_nodes, 0u);
}

TEST_CASE("radix_tree: randomized operations match std::unordered_map")
{
    alloc_stats stats;
    {
        tracked_tree tree { tracking_allocator<void> { &stats } };
        std::unordered_map<uint64_t, uint64_t> reference;
        std::mt19937_64 rng { 42 };

        // Mix dense low keys and sparse 64-bit keys
        std::vector<uint64_t> key_pool;
        key_pool.reserve(2000);
        for (uint64_t i = 0; i < 1000; ++i) {
            key_pool.push_back(i);
        }
        for (uint64_t i = 0; i < 1000; ++i) {
            key_pool.push_back(rng());
        }

        bool lookupsMatch = true;
        bool erasesMatch = true;

        for (int step = 0; step < 100000; ++step) {
            uint64_t const key = key_pool[rng() % key_pool.size()];
            switch (rng() % 4) {
            case 0:
            case 1: {
                uint64_t const val
                    = static_cast<uint64_t>(step) + 1; // non-zero
                CHECK(tree.insert(key, val));
                reference[key] = val;
                break;
            }
            case 2: {
                bool const tree_erased = tree.erase(key);
                bool const ref_erased = reference.erase(key) > 0;
                erasesMatch &= (tree_erased == ref_erased);
                break;
            }
            case 3: {
                auto* slot = tree.find(key);
                auto const it = reference.find(key);
                if (it == reference.end()) {
                    lookupsMatch
                        &= (slot == nullptr
                            || slot->load(std::memory_order_relaxed) == 0);
                } else {
                    lookupsMatch
                        &= (slot != nullptr
                            && slot->load(std::memory_order_relaxed)
                                   == it->second);
                }
                break;
            }
            }
        }

        CHECK(lookupsMatch);
        CHECK(erasesMatch);

        for (auto const& [k, v] : reference) {
            auto* slot = tree.find(k);
            REQUIRE(slot != nullptr);
            CHECK_EQ(slot->load(std::memory_order_relaxed), v);
        }

        // Erase remaining entries and verify every single node is pruned
        for (auto const& [k, v] : reference) {
            CHECK(tree.erase(k));
        }
        CHECK(tree.is_empty());
        CHECK_EQ(stats.live_nodes, 0u);
    }
    CHECK_EQ(stats.total_allocs, stats.total_deallocs);
}

TEST_CASE("radix_tree: concurrent lock-free readers with serialized writer")
{
    // Test concurrent lock-free find() while a writer inserts and updates keys
    // (without pruning nodes out from under readers, matching
    // immediate_reclaim_policy)
    radix_tree<uint64_t> tree;
    constexpr uint64_t num_keys = 4096;
    std::atomic<bool> done { false };
    std::atomic<bool> reader_ok { true };

    std::thread writer([&] {
        for (uint64_t pass = 1; pass <= 20; ++pass) {
            for (uint64_t k = 0; k < num_keys; ++k) {
                uint64_t const encoded = (k << 16) | pass;
                tree.insert(k, encoded);
            }
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([&, t] {
            uint64_t k = static_cast<uint64_t>(t);
            while (!done.load(std::memory_order_acquire)) {
                if (auto* slot = tree.find(k)) {
                    uint64_t const v = slot->load(std::memory_order_acquire);
                    if (v != 0 && (v >> 16) != k) {
                        reader_ok.store(false, std::memory_order_relaxed);
                    }
                }
                k = (k + 7) % num_keys;
            }
        });
    }

    writer.join();
    for (auto& r : readers) {
        r.join();
    }
    CHECK(reader_ok.load());
}
