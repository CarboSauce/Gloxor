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

/*
    @TODO: size could perhaps be replaced pointer to end of range
*/
struct PmmChunk
{
    glox::list_node list_node { };
    size_t size = 0;

    friend auto operator<=>(const PmmChunk& l, const PmmChunk& r) noexcept
    {
        return &l <=> &r;
    }
};

using pmmList = glox::intrusive_list<PmmChunk>;

static pmmList pmmCtx;
static u64 memorySize = 0;

namespace {

inline uintptr_t chunk_addr(const PmmChunk* chunk) noexcept
{
    return reinterpret_cast<uintptr_t>(chunk);
}

// Coalesce `chunk` with its immediate neighbors if physically adjacent
void try_coalesce(pmmList& list, pmmList::iterator it) noexcept
{
    PmmChunk* cur = &(*it);

    // 1. Try merging with next chunk
    auto next_it = it.next();
    if (next_it != list.end()) {
        PmmChunk* nxt = &(*next_it);
        if (chunk_addr(cur) + cur->size == chunk_addr(nxt)) {
            cur->size += nxt->size;
            list.erase(next_it);
        }
    }

    // 2. Try merging with previous chunk
    if (it != list.begin()) {
        auto prev_it = it.prev();
        PmmChunk* prv = &(*prev_it);
        if (chunk_addr(prv) + prv->size == chunk_addr(cur)) {
            prv->size += cur->size;
            list.erase(it);
        }
    }
}

void* alloc_from_chunk(
    pmmList& list,
    size_t chunkSize,
    usize pageCount
) noexcept
{
    if (pageCount == 0) {
        return nullptr;
    }
    const size_t needed = chunkSize * pageCount;

    for (auto it = list.begin(); it != list.end(); ++it) {
        PmmChunk* cur = &(*it);
        if (cur->size < needed) {
            continue;
        }

        cur->size -= needed;
        void* allocated = reinterpret_cast<void*>(chunk_addr(cur) + cur->size);
        if (cur->size == 0) {
            list.erase(it);
        }
        return reinterpret_cast<void*>(
            arch::to_virt(reinterpret_cast<vaddr>(allocated))
        );
    }

    return nullptr;
}

} // namespace

namespace gx {

void pmm_add_chunk(void* base, size_t length)
{
    GLOX_ASSERT(
        length % gx::pmmChunkSize == 0,
        "Pmm chunk length must be multiple of pmmChunkSize"
    );
    if (length == 0) {
        return;
    }

    const auto realBase = arch::to_virt(reinterpret_cast<vaddr>(base));
    auto* chunk = std::construct_at(reinterpret_cast<PmmChunk*>(realBase));
    chunk->size = length;
    memorySize += length;

    if (pmmCtx.is_empty()) {
        pmmCtx.push_back(chunk);
        return;
    }

    if (&pmmCtx.back() < chunk) {
        pmmCtx.push_back(chunk);
        try_coalesce(pmmCtx, --pmmCtx.end());
    } else if (&pmmCtx.front() > chunk) {
        pmmCtx.push_front(chunk);
        try_coalesce(pmmCtx, pmmCtx.begin());
    } else {
        // Find first chunk at a higher address and insert right before it
        auto it = pmmCtx.begin();
        while (it != pmmCtx.end() && &(*it) < chunk) {
            ++it;
        }
        auto inserted = pmmCtx.insert(it, chunk);
        try_coalesce(pmmCtx, inserted);
    }
}

void* page_alloc(usize pageCount)
{
    return alloc_from_chunk(pmmCtx, gx::pmmChunkSize, pageCount);
}

void* page_alloc_z(usize pageCount)
{
    auto* addr = page_alloc(pageCount);
    if (addr != nullptr) {
        glox::memset(addr, 0, pageCount * gx::pmmChunkSize);
    }
    return addr;
}

void page_dealloc(void* ptr, usize pageCount)
{
    if (ptr == nullptr) {
        return;
    }
    gx::pmm_add_chunk(ptr, gx::pmmChunkSize * pageCount);
}

} // namespace gx

#ifdef TEST
static bool test()
{
    auto startSize = pmmCtx.front().size;
    void* p = page_alloc();
    void* p2 = page_alloc();
    page_dealloc(p);
    auto nextH = &(*pmmCtx.begin().next());
    KTEST_EXPECT(pmmCtx.begin().next()->size == 0x1000);
    page_dealloc(p2);
    KTEST_EXPECT(pmmCtx.front().size == startSize);
    KTEST_EXPECT(&(*pmmCtx.begin().next()) > nextH);
    return true;
}
REGISTER_TEST("Page allocator", test);
#endif
