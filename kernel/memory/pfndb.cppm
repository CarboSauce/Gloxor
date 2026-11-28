export module gloxor.memory.pfndb;

import glox;
import gloxor.types;
import std;
import std.compat;
import gloxor.memory.virtmem;
import gloxor.memory.pmm;
import arch;
import gloxor.kinfo;
import gloxor.system.logging;

#include <glox/assert.hpp>

export namespace gx {
struct PfnDesc
{
    enum PfnDescType
    {
        TAG_BUDDY = 0x00,
        TAG_META = 0x01,
        TAG_SLAB = 0x02,
        TAG_LARGE = 0x03,
        TAG_LARGE_TAIL = 0x04,
        TAG_INVALID = 0x05,
        TAG_MASK = 0x1F
    };
    u64 entry;

    bool is_buddy() const
    {
        return (entry & TAG_MASK) == TAG_BUDDY;
    }
    bool is_meta() const
    {
        return (entry & TAG_MASK) == TAG_META;
    }
    bool is_slab() const
    {
        return (entry & TAG_MASK) == TAG_SLAB;
    }
    bool is_large() const
    {
        return (entry & TAG_MASK) == TAG_LARGE;
    }
    bool is_large_tail() const
    {
        return (entry & TAG_MASK) == TAG_LARGE_TAIL;
    }
    static PfnDesc make_reserved()
    {
        return { 0 };
    }
    static PfnDesc make_buddy_free(std::size_t o) noexcept
    {
        return { (u64(o) << 6) | 32 };
    }
    static PfnDesc make_allocated(std::size_t o) noexcept
    {
        return { u64(o) << 6 };
    }
    bool is_buddy_free() const noexcept
    {
        auto v = entry;
        return (v & 31) == 0 and (v & 32) != 0;
    }
    usize buddy_order() const noexcept
    {
        return entry >> 6;
    }

    static PfnDesc from_raw(std::uint64_t r) noexcept
    {
        return { r };
    }
    std::uint64_t raw() const noexcept
    {
        return entry;
    }
    void set_desc(PfnDesc desc)
    {
        entry = desc.entry;
    }
};

class PfndbManager
{
    // TODO: should be std::atomic, needs to change api of buddy allocator
    // or maybe should change PfndbEntry to atomic<u64> entry
    std::atomic<PfnDesc>* pfndbEntries;
    usize entryCount;

    static constexpr std::size_t PAGE_SIZE = 4096;
    static constexpr std::size_t PFNS_PER_PAGE = PAGE_SIZE / sizeof(PfnDesc);
    static constexpr std::size_t SEGMENT_SHIFT
        = 12 + std::countr_zero(PFNS_PER_PAGE);

public:
    [[nodiscard]] PfnDesc get_desc(std::uintptr_t pfn) const
    {
        GLOX_ASSERT(pfn < entryCount);

        return reinterpret_cast<std::atomic<PfnDesc> const*>(
            arch::PFN_DB_BASE
        )[pfn]
            .load();
    }
    void set_desc(std::uintptr_t pfn, PfnDesc desc)
    {
        auto* ptr
            = reinterpret_cast<std::atomic<PfnDesc>*>(arch::PFN_DB_BASE) + pfn;
        ptr->store(desc);
    }
    [[nodiscard]] void* pfn_to_virt(std::uintptr_t pfn) const
    {
        return (void*)arch::to_virt(pfn << 12);
    }
    [[nodiscard]] bool is_valid(std::uintptr_t pfn) const
    {
        return pfn < entryCount;
    }

    static constexpr std::uint64_t TAG_BUDDY = PfnDesc::TAG_BUDDY;
    static constexpr std::uint64_t TAG_META = PfnDesc::TAG_META;
    static constexpr std::uint64_t TAG_SLAB = PfnDesc::TAG_SLAB;
    static constexpr std::uint64_t TAG_LARGE = PfnDesc::TAG_LARGE;
    static constexpr std::uint64_t TAG_LARGE_TAIL = PfnDesc::TAG_LARGE_TAIL;
    static constexpr std::uint64_t TAG_INVALID = PfnDesc::TAG_INVALID;

    friend void init_pfndb();
};

constinit inline PfndbManager pfndbManager = { };

struct PfndbAccessor
{
    [[nodiscard]] PfnDesc get_desc(std::uintptr_t pfn) const
    {
        return pfndbManager.get_desc(pfn);
    }
    void set_desc(std::uintptr_t pfn, PfnDesc desc)
    {
        return pfndbManager.set_desc(pfn, desc);
    }
    [[nodiscard]] void* pfn_to_virt(std::uintptr_t pfn) const
    {
        return pfndbManager.pfn_to_virt(pfn);
    }
    [[nodiscard]] std::uintptr_t virt_to_pfn(void const* p) const
    {
        return arch::get_real_address((vaddr)p) >> 12;
    }
    [[nodiscard]] bool is_valid(std::uintptr_t pfn) const
    {
        return pfndbManager.is_valid(pfn);
    }

    static constexpr std::uint64_t TAG_BUDDY = PfnDesc::TAG_BUDDY;
    static constexpr std::uint64_t TAG_META = PfnDesc::TAG_META;
    static constexpr std::uint64_t TAG_SLAB = PfnDesc::TAG_SLAB;
    static constexpr std::uint64_t TAG_LARGE = PfnDesc::TAG_LARGE;
    static constexpr std::uint64_t TAG_LARGE_TAIL = PfnDesc::TAG_LARGE_TAIL;
    static constexpr std::uint64_t TAG_INVALID = PfnDesc::TAG_INVALID;
};
static_assert(glox::pfndb_accessor<PfndbAccessor>);

void init_pfndb()
{
    // this allocation code can be probably reworked
    // instead of using list make a segment array in deque fashion

    struct BackingMapRegion
    {
        uintptr_t base;
        usize length;
    };

    struct BackingMapRegionEntry
    {
        BackingMapRegion region;
        glox::list_node list_node;
    };
    glox::intrusive_list<BackingMapRegionEntry> backingMapRegions = { };
    constexpr usize pageSize = 4096;
    constexpr usize pfnsPerPage = pageSize / sizeof(PfnDesc);

    usize const base = arch::PFN_DB_BASE;

    auto isAvailable = [](auto const& region) {
        return region.type == BootInfo::MemTypes::usable
            || region.type == BootInfo::MemTypes::reclaimable;
    };

    // find the highest pfn
    usize maxPfn = 0;

    for (auto const& region : gx::machineInfo.mmapEntries) {
        if (!isAvailable(region))
            continue;

        maxPfn = std::max(
            maxPfn, static_cast<usize>((region.base + region.length) / pageSize)
        );
    }

    pfndbManager.entryCount = maxPfn;

    gx::println("[pfndb] % PFNs", maxPfn);

    // build a list of populated pfn db segments
    for (auto const& region : gx::machineInfo.mmapEntries) {
        if (!isAvailable(region) || !region.length)
            continue;

        usize const first
            = (region.base + pageSize - 1) / pageSize / pfnsPerPage;

        usize const last
            = ((region.base + region.length - 1) / pageSize) / pfnsPerPage;

        for (usize segment = first; segment <= last; ++segment) {
            bool exists = false;

            for (auto const& entry : backingMapRegions) {
                usize const start = (entry.region.base - base) / pageSize;

                usize const end = start + entry.region.length / pageSize;

                if (segment >= start && segment < end) {
                    exists = true;
                    break;
                }
            }

            if (exists)
                continue;

            auto* entry = static_cast<BackingMapRegionEntry*>(
                gx::boot_alloc(sizeof(BackingMapRegionEntry))
            );

            new (entry) BackingMapRegionEntry {
                .region = {
                    .base = base + segment * pageSize,
                    .length = pageSize,
                },
                .list_node = {},
            };

            backingMapRegions.push_back(entry);
        }
    }

    // allocate and map backing pages
    usize mappedSegments = 0;

    for (auto const& entry : backingMapRegions) {
        usize const start = (entry.region.base - base) / pageSize;

        usize const count = entry.region.length / pageSize;

        for (usize i = 0; i < count; ++i) {
            auto* page = static_cast<PfnDesc*>(gx::page_alloc());

            GLOX_ASSERT(page != nullptr);

            kAddrSpace.map(
                base + (start + i) * pageSize,
                arch::get_real_address(reinterpret_cast<vaddr>(page)),
                arch::vmem::PagePrivileges::readWrite,
                arch::vmem::PageCaching::writeCombine
            );

            for (usize j = 0; j < pfnsPerPage; ++j) {
                new (&page[j]) PfnDesc {
                    .entry = PfnDesc::TAG_INVALID,
                };
            }

            ++mappedSegments;
        }
    }

    // initialize available physical pages
    pfndbManager.pfndbEntries = reinterpret_cast<std::atomic<PfnDesc>*>(base);

    for (auto const& region : gx::machineInfo.mmapEntries) {
        if (!isAvailable(region))
            continue;

        usize const first = (region.base + pageSize - 1) / pageSize;

        usize const last = (region.base + region.length) / pageSize;

        for (usize pfn = first; pfn < last; ++pfn)
            pfndbManager.set_desc(pfn, PfnDesc::make_allocated(0));
    }

    backingMapRegions.clear([](auto* ptr) {
        gx::boot_dealloc(ptr, sizeof(*ptr));
    });

    gx::println("[pfndb] initialized, % segments mapped", mappedSegments);
}
} // namespace gx
