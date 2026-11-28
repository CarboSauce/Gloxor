module gloxor.memory.pmm;
import glox;
import gloxor.memory;
import gloxor.types;
import gloxor.system;
import arch;
import std.compat;
import std;

#include "glox/assert.hpp"
#include "gloxor/test.hpp"

using namespace gx;

struct BootChunk
{
    glox::list_node list_node { };
    size_t size = 0;

    friend auto operator<=>(BootChunk const& l, BootChunk const& r) noexcept
    {
        return &l <=> &r;
    }
};

using bootList = glox::intrusive_list<BootChunk>;

static bootList bootCtx;
static u64 memorySize = 0;
constexpr auto SIZE_MAX = std::numeric_limits<std::size_t>::max();
constexpr auto UINTPTR_MAX = std::numeric_limits<std::uintptr_t>::max();
static constexpr bool TRACK_ALLOCATIONS = false;

namespace {

inline uintptr_t chunk_addr(BootChunk const* chunk) noexcept
{
    return reinterpret_cast<uintptr_t>(chunk);
}

constexpr bool is_power_of_two(usize value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

constexpr uintptr_t align_up(uintptr_t value, usize alignment) noexcept
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// Coalesce `chunk` with its immediate neighbors if physically adjacent.
void try_coalesce(bootList& list, bootList::iterator it) noexcept
{
    BootChunk* cur = &(*it);

    // Merge with next chunk.
    auto next_it = it.next();
    if (next_it != list.end()) {
        BootChunk* nxt = &(*next_it);

        if (chunk_addr(cur) + cur->size == chunk_addr(nxt)) {
            cur->size += nxt->size;
            list.erase(next_it);
        }
    }

    // Merge with previous chunk.
    if (it != list.begin()) {
        auto prev_it = it.prev();
        BootChunk* prv = &(*prev_it);

        if (chunk_addr(prv) + prv->size == chunk_addr(cur)) {
            prv->size += cur->size;
            list.erase(it);
        }
    }
}

// Insert a free chunk into the sorted list.
void insert_chunk(bootList& list, uintptr_t base, usize size) noexcept
{
    if (size < sizeof(BootChunk)) {
        return;
    }

    auto* chunk = std::construct_at(reinterpret_cast<BootChunk*>(base));

    chunk->size = size;

    if (list.is_empty()) {
        list.push_back(chunk);
        return;
    }

    auto it = list.begin();

    while (it != list.end() && chunk_addr(&(*it)) < base) {
        ++it;
    }

    auto inserted = list.insert(it, chunk);
    try_coalesce(list, inserted);
}

// Allocate arbitrary bytes with arbitrary alignment.
void* alloc_from_chunk(bootList& list, usize size, usize alignment) noexcept
{
    if (size == 0) {
        return nullptr;
    }

    GLOX_ASSERT(
        is_power_of_two(alignment),
        "Boot allocation alignment must be a power of two"
    );

    // Ensure the allocation can hold free-list metadata when returned.
    // This is necessary because allocations can later be freed.
    usize const allocSize = size;

    for (auto it = list.begin(); it != list.end(); ++it) {
        BootChunk* cur = &(*it);

        uintptr_t const base = chunk_addr(cur);
        uintptr_t const end = base + cur->size;

        if (end < base) {
            continue;
        }

        // Leave enough room for metadata before the allocation
        // if alignment introduces a prefix.
        uintptr_t allocated = align_up(base, alignment);

        if (allocated < base) {
            continue;
        }

        usize prefix = allocated - base;

        // A prefix smaller than BootChunk cannot become a free chunk.
        // Move the allocation forward until the prefix is usable.
        if (prefix != 0 && prefix < sizeof(BootChunk)) {
            allocated = align_up(base + sizeof(BootChunk), alignment);

            if (allocated < base) {
                continue;
            }

            prefix = allocated - base;
        }

        if (allocated > end || allocSize > end - allocated) {
            continue;
        }

        uintptr_t const allocationEnd = allocated + allocSize;
        usize const suffix = end - allocationEnd;

        // A suffix must either be large enough to hold metadata,
        // or be absorbed into the allocation.
        usize actualSize = allocSize;

        if (suffix != 0 && suffix < sizeof(BootChunk)) {
            actualSize += suffix;
        }

        // Remove the original chunk before rebuilding its free parts.
        list.erase(it);

        // Preserve prefix.
        if (prefix != 0) {
            insert_chunk(list, base, prefix);
        }

        // Preserve suffix.
        if (end > allocationEnd && suffix >= sizeof(BootChunk)) {
            insert_chunk(list, allocationEnd, suffix);
        }

        return reinterpret_cast<void*>(
            arch::to_virt(static_cast<vaddr>(allocated))
        );
    }

    return nullptr;
}

} // namespace

namespace gx {
void boot_reclaim(glox::function_ref<void(void*, usize)> cb)
{
    while (not bootCtx.is_empty()) {
        auto& ctx = bootCtx.front();
        auto start = &ctx;
        auto size = ctx.size;
        bootCtx.erase(bootCtx.begin());
        if constexpr (TRACK_ALLOCATIONS) {
            println("[boot_alloc] Reclaiming (%x,%x)", start, size);
        }
        cb(start, size);
    }
}

// Add a free memory region.
void boot_add_chunk(void* base, size_t length)
{
    if (length == 0) {
        return;
    }

    GLOX_ASSERT(length >= sizeof(BootChunk), "Boot chunk too small");

    auto const realBase
        = static_cast<uintptr_t>(arch::to_virt(reinterpret_cast<vaddr>(base)));

    GLOX_ASSERT(
        realBase <= std::numeric_limits<uintptr_t>::max() - length,
        "Boot chunk address overflow"
    );

    insert_chunk(bootCtx, realBase, length);

    memorySize += length;
}

// Allocate arbitrary bytes with specified alignment.
void* boot_alloc(usize size, usize alignment)
{
    if constexpr (TRACK_ALLOCATIONS) {
        gx::println(
            "[boot_alloc] Allocating % with alignment %", size, alignment
        );
    }
    return alloc_from_chunk(bootCtx, size, alignment);
}

// Allocate and zero arbitrary bytes.
void* boot_alloc_z(usize size, usize alignment)
{
    auto* addr = boot_alloc(size, alignment);

    if (addr != nullptr) {
        glox::memset(addr, 0, size);
    }

    return addr;
}

// Page allocation wrappers.
void* page_alloc(usize pageCount)
{
    if (pageCount == 0 || pageCount > SIZE_MAX / gx::pmmChunkSize) {
        return nullptr;
    }

    return boot_alloc(gx::pmmChunkSize * pageCount, gx::pmmChunkSize);
}

void* page_alloc_z(usize pageCount)
{
    if (pageCount == 0 || pageCount > SIZE_MAX / gx::pmmChunkSize) {
        return nullptr;
    }

    return boot_alloc_z(gx::pmmChunkSize * pageCount, gx::pmmChunkSize);
}

// Return memory to the free list.
void boot_dealloc(void* ptr, usize size)
{
    if (ptr == nullptr || size == 0) {
        return;
    }
    if constexpr (TRACK_ALLOCATIONS) {
        gx::println("[boot_alloc] Deallocating %x with size %", ptr, size);
    }

    uintptr_t const base = reinterpret_cast<uintptr_t>(ptr);

    GLOX_ASSERT(
        size >= sizeof(BootChunk), "Deallocated chunk too small for metadata"
    );

    GLOX_ASSERT(
        base <= UINTPTR_MAX - size, "Deallocated chunk address overflow"
    );

    insert_chunk(bootCtx, base, size);
}

// Page deallocation wrapper.
void page_dealloc(void* ptr, usize pageCount)
{
    if (ptr == nullptr || pageCount == 0) {
        return;
    }

    GLOX_ASSERT(
        pageCount <= SIZE_MAX / gx::pmmChunkSize,
        "Page deallocation size overflow"
    );

    boot_dealloc(ptr, gx::pmmChunkSize * pageCount);
}

} // namespace gx

#ifdef TEST

namespace {

bool boot_allocator_test()
{
    // Save initial free memory.
    usize const initialChunkCount = [&] {
        usize count = 0;
        for (auto it = bootCtx.begin(); it != bootCtx.end(); ++it) {
            ++count;
        }
        return count;
    }();

    u64 const initialMemorySize = memorySize;

    // Test 1: Arbitrary allocation size.
    {
        void* ptr = boot_alloc(128);

        KTEST_EXPECT(ptr != nullptr);
        KTEST_EXPECT(
            reinterpret_cast<uintptr_t>(ptr) % alignof(std::max_align_t) == 0
        );

        boot_dealloc(ptr, 128);
    }

    // Test 2: Explicit alignment.
    {
        constexpr usize alignment = 64;

        void* ptr = boot_alloc(128, alignment);

        KTEST_EXPECT(ptr != nullptr);
        KTEST_EXPECT(reinterpret_cast<uintptr_t>(ptr) % alignment == 0);

        boot_dealloc(ptr, 128);
    }

    // Test 3: Page alignment with arbitrary size.
    {
        constexpr usize size = 5000;

        void* ptr = boot_alloc(size, gx::pmmChunkSize);

        KTEST_EXPECT(ptr != nullptr);
        KTEST_EXPECT(reinterpret_cast<uintptr_t>(ptr) % gx::pmmChunkSize == 0);

        boot_dealloc(ptr, size);
    }
    // Test 4: Zeroed allocation.
    {
        constexpr usize size = 128;

        auto* ptr = static_cast<u8*>(boot_alloc_z(size, 16));

        KTEST_EXPECT(ptr != nullptr);

        bool zeroed = true;

        for (usize i = 0; i < size; ++i) {
            if (ptr[i] != 0) {
                zeroed = false;
                break;
            }
        }

        KTEST_EXPECT(zeroed);

        boot_dealloc(ptr, size);
    }

    // Test 5: Multiple allocations must not overlap.
    {
        void* a = boot_alloc(128, 16);
        void* b = boot_alloc(256, 64);
        void* c = boot_alloc(512, 128);

        KTEST_EXPECT(a != nullptr);
        KTEST_EXPECT(b != nullptr);
        KTEST_EXPECT(c != nullptr);

        auto const aStart = reinterpret_cast<uintptr_t>(a);
        auto const bStart = reinterpret_cast<uintptr_t>(b);
        auto const cStart = reinterpret_cast<uintptr_t>(c);

        KTEST_EXPECT(aStart + 128 <= bStart || bStart + 256 <= aStart);

        KTEST_EXPECT(bStart + 256 <= cStart || cStart + 512 <= bStart);

        boot_dealloc(a, 128);
        boot_dealloc(b, 256);
        boot_dealloc(c, 512);
    }

    // Test 6: Coalescing after deallocation.
    {
        void* a = boot_alloc(128, 16);
        void* b = boot_alloc(128, 16);

        KTEST_EXPECT(a != nullptr);
        KTEST_EXPECT(b != nullptr);

        auto const aStart = reinterpret_cast<uintptr_t>(a);
        auto const bStart = reinterpret_cast<uintptr_t>(b);

        // Only test adjacent allocations.
        if (aStart + 128 == bStart) {
            boot_dealloc(a, 128);
            boot_dealloc(b, 128);

            bool foundMerged = false;

            for (auto it = bootCtx.begin(); it != bootCtx.end(); ++it) {
                auto* chunk = &(*it);

                if (reinterpret_cast<uintptr_t>(chunk) == aStart
                    && chunk->size >= 256) {
                    foundMerged = true;
                    break;
                }
            }

            KTEST_EXPECT(foundMerged);
        } else {
            boot_dealloc(a, 128);
            boot_dealloc(b, 128);
        }
    }

    // Test 7: Page allocation wrapper.
    {
        void* ptr = page_alloc(2);

        KTEST_EXPECT(ptr != nullptr);
        KTEST_EXPECT(reinterpret_cast<uintptr_t>(ptr) % gx::pmmChunkSize == 0);

        page_dealloc(ptr, 2);
    }

    // Test 8: Invalid allocation arguments.
    {
        KTEST_EXPECT(boot_alloc(0) == nullptr);
    }

    // Verify the allocator has returned to its initial state.
    usize finalChunkCount = 0;

    for (auto it = bootCtx.begin(); it != bootCtx.end(); ++it) {
        ++finalChunkCount;
    }

    KTEST_EXPECT(finalChunkCount == initialChunkCount);
    KTEST_EXPECT(memorySize == initialMemorySize);

    return true;
}

} // namespace

REGISTER_TEST("Boot allocator", boot_allocator_test);

#endif
