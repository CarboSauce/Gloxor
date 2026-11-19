#ifndef USE_MODULES
#include "glox/buddy_alloc.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#else
import std;
import glox;
#endif
#include "doctest.h"

using namespace glox;

namespace {
// 8-byte tagged descriptor
struct alignas(8) test_pfn_desc
{
    enum class tag : std::uintptr_t
    {
        reserved = 0b000,
        buddy_free = 0b001,
        allocated = 0b010,
        memdesc = 0b100,
    };

    static constexpr std::uintptr_t TAG_MASK = 0b111;
    std::uintptr_t raw = 0;

    [[nodiscard]] constexpr tag kind() const noexcept
    {
        return static_cast<tag>(raw & TAG_MASK);
    }

    static constexpr test_pfn_desc make_reserved() noexcept
    {
        return test_pfn_desc { 0 };
    }

    static constexpr test_pfn_desc make_buddy_free(std::size_t order) noexcept
    {
        return test_pfn_desc { (static_cast<std::uintptr_t>(order & 0xFFu) << 8)
                               | static_cast<std::uintptr_t>(tag::buddy_free) };
    }

    static constexpr test_pfn_desc make_allocated(std::size_t order) noexcept
    {
        return test_pfn_desc { (std::uintptr_t { 1 } << 32)
                               | (static_cast<std::uintptr_t>(order & 0xFFu)
                                   << 8)
                               | static_cast<std::uintptr_t>(tag::allocated) };
    }

    [[nodiscard]] constexpr bool is_buddy_free() const noexcept
    {
        return kind() == tag::buddy_free;
    }

    [[nodiscard]] constexpr std::size_t buddy_order() const noexcept
    {
        return static_cast<std::size_t>((raw >> 8) & 0xFFu);
    }
};

static_assert(sizeof(test_pfn_desc) == 8);
static_assert(pfn_descriptor<test_pfn_desc>);

// 16-bit descriptor verifying concept genericity
struct compact_pfn_desc
{
    std::uint16_t raw = 0;

    static constexpr compact_pfn_desc make_reserved() noexcept
    {
        return { 0 };
    }
    static constexpr compact_pfn_desc make_buddy_free(
        std::size_t order
    ) noexcept
    {
        return { static_cast<std::uint16_t>(0x8000u | (order & 0xFFu)) };
    }
    static constexpr compact_pfn_desc make_allocated(std::size_t order) noexcept
    {
        return { static_cast<std::uint16_t>(0x4000u | (order & 0xFFu)) };
    }

    [[nodiscard]] constexpr bool is_buddy_free() const noexcept
    {
        return (raw & 0x8000u) != 0;
    }
    [[nodiscard]] constexpr std::size_t buddy_order() const noexcept
    {
        return raw & 0xFFu;
    }
};

static_assert(pfn_descriptor<compact_pfn_desc>);

template <pfn_descriptor Desc = test_pfn_desc>
struct mock_pfndb
{
    static constexpr std::size_t PAGE_SIZE = 4096;
    static constexpr std::size_t PFNS_PER_PAGE = PAGE_SIZE / sizeof(Desc);
    static constexpr std::size_t SEGMENT_SHIFT
        = 12 + std::countr_zero(PFNS_PER_PAGE);

    std::uintptr_t hhdm_offset = 0;
    std::unordered_map<std::size_t, std::vector<Desc>> segments;

    void map_range(std::uintptr_t phys_addr, std::size_t size)
    {
        if (size == 0) {
            return;
        }
        const std::size_t firstSeg = phys_addr >> SEGMENT_SHIFT;
        const std::size_t lastSeg = (phys_addr + size - 1) >> SEGMENT_SHIFT;
        for (std::size_t seg = firstSeg; seg <= lastSeg; ++seg) {
            segments.try_emplace(seg, PFNS_PER_PAGE, Desc::make_reserved());
        }
    }

    [[nodiscard]] bool is_valid(std::uintptr_t pfn) const noexcept
    {
        return segments.contains(pfn >> (SEGMENT_SHIFT - 12));
    }

    [[nodiscard]] Desc& operator[](std::uintptr_t pfn) noexcept
    {
        return segments.at(
            pfn >> (SEGMENT_SHIFT - 12)
        )[pfn & (PFNS_PER_PAGE - 1)];
    }

    [[nodiscard]] const Desc& operator[](std::uintptr_t pfn) const noexcept
    {
        return segments.at(
            pfn >> (SEGMENT_SHIFT - 12)
        )[pfn & (PFNS_PER_PAGE - 1)];
    }

    [[nodiscard]] void* pfn_to_virt(std::uintptr_t pfn) const noexcept
    {
        return reinterpret_cast<void*>((pfn << 12) + hhdm_offset);
    }
};

static_assert(pfndb_accessor<mock_pfndb<test_pfn_desc>>);
static_assert(pfndb_accessor<mock_pfndb<compact_pfn_desc>>);

struct simulated_ram
{
    std::vector<std::byte> storage;
    std::uintptr_t phys_base;
    std::uintptr_t hhdm_offset;

    simulated_ram(std::size_t bytes, std::uintptr_t fake_phys_base)
        : storage(bytes + 4096)
        , phys_base(fake_phys_base)
    {
        const auto raw = reinterpret_cast<std::uintptr_t>(storage.data());
        const auto aligned = (raw + 4095) & ~std::uintptr_t { 4095 };
        hhdm_offset = aligned - phys_base;
    }

    [[nodiscard]] void* phys_to_ptr(std::uintptr_t phys) const noexcept
    {
        return reinterpret_cast<void*>(phys + hhdm_offset);
    }
};
} // namespace

TEST_CASE("buddy_allocator basic alloc, orderless free, and coalescing")
{
    constexpr std::uintptr_t base = 0x200000;
    constexpr std::size_t ramBytes = 64 * 4096;
    simulated_ram ram(ramBytes, base);

    mock_pfndb<test_pfn_desc> db;
    db.hhdm_offset = ram.hhdm_offset;
    db.map_range(base, ramBytes);

    buddy_allocator<mock_pfndb<test_pfn_desc>> alloc(std::move(db));
    alloc.add_region({ base, ramBytes });

    REQUIRE(alloc.free_pages() == 64);
    REQUIRE(alloc.free_bytes() == 64 * 4096);

    const std::uintptr_t p1 = alloc.alloc_order(2); // 4 pages
    const std::uintptr_t p2 = alloc.alloc_order(0); // 1 page
    REQUIRE(p1 != 0);
    REQUIRE(p2 != 0);
    REQUIRE(alloc.accessor()[p1 >> 12].kind() == test_pfn_desc::tag::allocated);
    REQUIRE(alloc.accessor()[p1 >> 12].buddy_order() == 2);
    REQUIRE(alloc.free_pages() == 59);

    std::memset(ram.phys_to_ptr(p1), 0xAB, 4 * 4096);
    std::memset(ram.phys_to_ptr(p2), 0xCD, 4096);

    // orderless free reads order directly from the descriptor
    alloc.free(p1);
    alloc.free(p2);
    REQUIRE(alloc.free_pages() == 64);

    // coalesces back into a single 64 page (order 6) block
    const std::uintptr_t full = alloc.alloc_order(6);
    REQUIRE(full == base);
    REQUIRE(alloc.free_pages() == 0);
    REQUIRE(alloc.alloc_order(0) == 0); // OOM returns 0

    alloc.free(full);
    REQUIRE(alloc.free_pages() == 64);
}

TEST_CASE("buddy_allocator set_accessor and alloc_bytes/free_bytes")
{
    constexpr std::uintptr_t base = 0x800000;
    constexpr std::size_t ramBytes = 32 * 4096;
    simulated_ram ram(ramBytes, base);

    mock_pfndb<compact_pfn_desc> db;
    db.hhdm_offset = ram.hhdm_offset;
    db.map_range(base, ramBytes);

    buddy_allocator<mock_pfndb<compact_pfn_desc>> alloc;
    alloc.set_accessor(std::move(db));
    alloc.add_region(base, ramBytes);

    // 12 kb rounds up to 3 pages -> order 2 (4 pages = 16 kb)
    const std::uintptr_t p = alloc.alloc_bytes(12 * 1024);
    REQUIRE(p != 0);
    REQUIRE(alloc.free_pages() == 28);

    alloc.free_bytes(p, 12 * 1024);
    REQUIRE(alloc.free_pages() == 32);
}

TEST_CASE(
    "buddy_allocator add_region trims unaligned chunks and coalesces adjacent "
    "regions"
)
{
    constexpr std::uintptr_t base = 0x400000;
    constexpr std::size_t ramBytes = 128 * 4096;
    simulated_ram ram(ramBytes, base);

    mock_pfndb<> db;
    db.hhdm_offset = ram.hhdm_offset;
    db.map_range(base, ramBytes);

    buddy_allocator<mock_pfndb<>> alloc(std::move(db));

    // unaligned [0x40007b, 0x404000 - 100) -> 3 pages
    alloc.add_region(base + 0x7B, 4 * 4096 - 100);
    REQUIRE(alloc.free_pages() == 3);

    // Adjacent [0x404000, 0x408000) = 4 pages
    alloc.add_region(base + 4 * 4096, 4 * 4096);
    REQUIRE(alloc.free_pages() == 7);

    // prepend [0x400000, 0x401000) = 1 page -> completes [0x400000, 0x408000) =
    // 8 pages (order 3)
    alloc.add_region(base, 4096);
    REQUIRE(alloc.free_pages() == 8);

    const std::uintptr_t merged = alloc.alloc_order(3);
    REQUIRE(merged == base);
    alloc.free(merged);
    REQUIRE(alloc.free_pages() == 8);
}

TEST_CASE("buddy_allocator skips physical page 0x0 sentinel")
{
    constexpr std::uintptr_t base = 0x0;
    constexpr std::size_t ramBytes = 16 * 4096;
    simulated_ram ram(ramBytes, base);

    mock_pfndb<> db;
    db.hhdm_offset = ram.hhdm_offset;
    db.map_range(base, ramBytes);

    buddy_allocator<mock_pfndb<>> alloc(std::move(db));
    alloc.add_region(base, ramBytes);

    // page 0x0 is reserved so 0 stays a safe
    // oom return sentinel -> 15 usable pages
    REQUIRE(alloc.free_pages() == 15);

    std::vector<std::uintptr_t> pages;
    while (const auto p = alloc.alloc_order(0)) {
        REQUIRE(p >= 0x1000);
        pages.push_back(p);
    }
    REQUIRE(pages.size() == 15);

    for (auto p : pages) {
        alloc.free(p);
    }
    REQUIRE(alloc.free_pages() == 15);
}

TEST_CASE(
    "buddy_allocator safely handles MMIO holes across unmapped 2 MiB segments"
)
{
    constexpr std::uintptr_t seg1Phys = 0x200000;
    constexpr std::uintptr_t seg2Phys = 0x10000000;
    constexpr std::size_t segSize = 512 * 4096; // full 2 mb segment

    simulated_ram ram1(segSize, seg1Phys);
    simulated_ram ram2(segSize, seg2Phys);

    struct dual_seg_pfndb
    {
        std::uintptr_t seg1_pfn_start;
        std::uintptr_t seg1_hhdm;
        std::uintptr_t seg2_pfn_start;
        std::uintptr_t seg2_hhdm;
        test_pfn_desc seg1_meta[512] { };
        test_pfn_desc seg2_meta[512] { };

        [[nodiscard]] bool is_valid(std::uintptr_t pfn) const noexcept
        {
            return (pfn >= seg1_pfn_start && pfn < seg1_pfn_start + 512)
                || (pfn >= seg2_pfn_start && pfn < seg2_pfn_start + 512);
        }

        [[nodiscard]] test_pfn_desc& operator[](std::uintptr_t pfn) noexcept
        {
            if (pfn >= seg1_pfn_start && pfn < seg1_pfn_start + 512) {
                return seg1_meta[pfn - seg1_pfn_start];
            }
            return seg2_meta[pfn - seg2_pfn_start];
        }

        [[nodiscard]] const test_pfn_desc& operator[](
            std::uintptr_t pfn
        ) const noexcept
        {
            if (pfn >= seg1_pfn_start && pfn < seg1_pfn_start + 512) {
                return seg1_meta[pfn - seg1_pfn_start];
            }
            return seg2_meta[pfn - seg2_pfn_start];
        }

        [[nodiscard]] void* pfn_to_virt(std::uintptr_t pfn) const noexcept
        {
            if (pfn >= seg1_pfn_start && pfn < seg1_pfn_start + 512) {
                return reinterpret_cast<void*>((pfn << 12) + seg1_hhdm);
            }
            return reinterpret_cast<void*>((pfn << 12) + seg2_hhdm);
        }
    };

    static_assert(pfndb_accessor<dual_seg_pfndb>);

    buddy_allocator<dual_seg_pfndb> alloc(
        dual_seg_pfndb {
            seg1Phys >> 12, ram1.hhdm_offset, seg2Phys >> 12, ram2.hhdm_offset }
    );

    alloc.add_region({ seg1Phys, segSize });
    alloc.add_region({ seg2Phys, segSize });
    REQUIRE(alloc.free_pages() == 1024);

    const std::uintptr_t b1 = alloc.alloc_order(9);
    const std::uintptr_t b2 = alloc.alloc_order(9);
    REQUIRE(b1 != 0);
    REQUIRE(b2 != 0);
    REQUIRE(alloc.free_pages() == 0);

    alloc.free_order(b1, 9);
    alloc.free_order(b2, 9);
    REQUIRE(alloc.free_pages() == 1024);
}

TEST_CASE("buddy_allocator multi-threaded stress")
{
    constexpr std::uintptr_t base = 0x1000000;
    constexpr std::size_t totalPages = 512;
    simulated_ram ram(totalPages * 4096, base);

    mock_pfndb<> db;
    db.hhdm_offset = ram.hhdm_offset;
    db.map_range(base, totalPages * 4096);

    buddy_allocator<mock_pfndb<>, std::mutex> alloc(std::move(db));
    alloc.add_region(base, totalPages * 4096);

    constexpr int numThreads = 6;
    constexpr int iterations = 2'000;

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, tid = t]() {
            std::vector<std::uintptr_t> held;
            held.reserve(16);

            for (int i = 0; i < iterations; ++i) {
                const std::size_t order
                    = static_cast<std::size_t>((i + tid) % 4);
                if (const auto addr = alloc.alloc_order(order); addr != 0) {
                    std::memset(
                        ram.phys_to_ptr(addr),
                        static_cast<int>(tid + 1),
                        std::size_t { 4096 } << order
                    );
                    held.push_back(addr);
                }

                if (held.size() >= 8) {
                    alloc.free(held.back());
                    held.pop_back();
                }
            }

            for (auto addr : held) {
                alloc.free(addr);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    REQUIRE(alloc.free_pages() == totalPages);
}
