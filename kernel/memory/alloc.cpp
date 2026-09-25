#include "alloc.hpp"
#include "glox/assert.hpp"
#include "gloxor/mutex.hpp"
#include "llallocator.hpp"
#include "memory/pmm.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include <glox/array.hpp>
#include <glox/math.hpp>
#include <glox/mutex.hpp>
#include <glox/util.hpp>
#include <gloxor/test.hpp>

/*
    Allocator idea is pretty simple, its similar in idea to slab
    Its segregated fit with freelist of free slots in a bucket
    Metadata is stored in a unrolled linked list that fits the page,
    so you can construct it in the buffer provided by pageAlloc()
    Alloc calculates bucket index from the given size, and pops address
    from bucket with free slot, if there isnt a free list, new one is allocated
    Dealloc is pricy as it requires traversal of all entries of the given
   address
    TODO:
    -	Traversing unrolled list of buckets in case of allocation might be slow
        if lot of buckets are full, reconsider adding full list to keep track
        of allocated buckets, so we dont lose the information of them being
   allocated -	Freeing is costly, perhaps one could have FIFO/HashTable to
   speedup freeing -	Massive fragmentation might occur if lot of chunks are
   partially filled
*/
void* operator new(size_t size)
{
    return gx::memalloc(size);
}

void* operator new[](size_t size)
{
    return gx::memalloc(size);
}

void operator delete(void* p, size_t size)
{
    gx::memdealloc(p, size);
}

void operator delete[](void* p, size_t size)
{
    gx::memdealloc(p, size);
}

struct Freelist
{
    Freelist* next;
    struct Iterator
    {
        Freelist* it;
        auto operator++()
        {
            return it = it->next;
        }
        friend auto operator<=>(Iterator, Iterator) = default;
        auto& operator*() const
        {
            return *it;
        }
        auto& operator->() const
        {
            return it;
        }
    };
    Iterator begin()
    {
        return { this };
    }
    Iterator end()
    {
        return { nullptr };
    }
};
struct ChunkPtr
{
    usize bytesUsed;
    Freelist* list;
    constexpr bool is_full() const
    {
        return bytesUsed == gx::pmmChunkSize;
    }
};
struct Metadata
{
    static constexpr usize chunkCount
        = (gx::pmmChunkSize - (sizeof(Metadata*) + sizeof(usize)))
        / sizeof(ChunkPtr);
    using chunkHeadersT = glox::array<ChunkPtr, chunkCount>;
    Metadata* next;
    usize usedChunks;
    chunkHeadersT chunkHeaders;
};

static_assert(sizeof(Metadata) == gx::pmmChunkSize);
struct MetadataCtx
{
    usize leftover; // how many bytes left
    Metadata* list;
};

constexpr glox::array<size_t, 8>
    tinyBuckets { 16, 32, 64, 128, 256, 512, 1024, 2048 };
struct HeapCtx
{
    static constexpr usize bucketCount = tinyBuckets.size;
    usize totalPages;
    //	freelist* bigAllocList;
    glox::array<MetadataCtx, bucketCount> buckets;
};

static HeapCtx globalHeap;
// alligns number upwards to next power of 2
// if one uses it for bucket index calculation, make sure to offset it
inline auto align_up_pow2(usize x)
{
    return x == 1 ? 1 : sizeof(usize) * 8 - glox::clz(x - 1);
}

inline usize size2bucket(usize size)
{
    gloxAssert(size != 0);
    if (size <= tinyBuckets[0])
        return 0;
    return align_up_pow2(size) - 4;
}

void* big_alloc(usize size)
{
    return gx::PmmAllocator::alloc(size);
}

inline bool init_chunk(ChunkPtr& list, usize bucketSize)
{
    auto freshAddr = (Freelist*)gx::page_alloc();
    if (freshAddr == nullptr)
        return false;
    list.list = freshAddr;
    auto offset = bucketSize;
    for (usize i = 0; i < gx::pmmChunkSize; i += offset) {
        *freshAddr = { (Freelist*)((uintptr)freshAddr + offset) };
        freshAddr = freshAddr->next;
    }
    freshAddr->next = nullptr;
    list.bytesUsed = 0;
    return true;
}

inline glox::pair<void*, bool>
alloc_from_chunk(Metadata::chunkHeadersT& chunks, usize bucketSize)
{
    for (auto&& it : chunks) {
        // completely borked, quickly fix
        if (it.list == nullptr) {
            if (!init_chunk(it, bucketSize))
                return { nullptr, true };
        }
        // we need tagged pointers to mark if page is full or not
        // fallthrough from previous branch
        if (!it.is_full()) {
            auto tmp = it.list;
            it.list = it.list->next;
            it.bytesUsed += bucketSize;
            return { tmp, true };
        }
    }
    return { nullptr, false };
}

inline bool free_chunk(ChunkPtr& it, void* ptr, usize size)
{
    auto newentry = (Freelist*)ptr;
    it.bytesUsed -= size;
    if (it.bytesUsed == 0) {
        gx::page_dealloc((void*)ALIGN((uintptr)it.list, gx::pmmChunkSize));
        return true;
    }
    newentry->next = it.list;
    it.list = newentry;
    return true;
}

inline bool free_mem(void* ptr, usize size)
{
    if (ptr == nullptr or size == 0) {
        return false;
    }
    if (size > tinyBuckets[tinyBuckets.size - 1]) {
        return gx::PmmAllocator::dealloc(ptr, size), true;
    }
    auto tbindex = size2bucket(size);
    auto realsize = tinyBuckets[tbindex];
    auto* curlist = globalHeap.buckets[tbindex].list;
    auto allignedptr = ALIGN((uintptr)ptr, gx::pmmChunkSize);
    for (auto iter = curlist; iter; iter = iter->next) {
        for (auto& it : iter->chunkHeaders) {
            if (allignedptr == ALIGN((uintptr)it.list, gx::pmmChunkSize))
                return free_chunk(it, ptr, realsize);
        }
    }
    return false;
}

inline void* alloc_mem(usize size)
{
    if (size == 0)
        return nullptr;
    if (size > tinyBuckets[tinyBuckets.size - 1]) {
        return big_alloc(size);
    }
    auto index = size2bucket(size);
    auto bucketSize = tinyBuckets[index];
    auto& curList = globalHeap.buckets[index].list;
    // gloxDebugLog("Memalloc(", bucketSize, ")\n");
    auto* iter = curList;
    // TODO: Rewrite into inf loop, as its probably clearer
    if (iter == nullptr) {
        iter = (Metadata*)gx::page_alloc();
        if (iter == nullptr)
            return nullptr;
        curList = iter;
    }
    auto iterNext = iter;
    do {
        iter = iterNext;
        if (auto val = alloc_from_chunk(iter->chunkHeaders, bucketSize);
            val.second) {
            return val.first;
        }
        iterNext = iter->next;
    } while (iterNext);
    iterNext = (Metadata*)gx::page_alloc();
    if (iterNext == nullptr)
        return nullptr;
    iter->next = iterNext;
    return alloc_from_chunk(iter->chunkHeaders, bucketSize).first;
}

namespace gx {

void* memalloc(usize size)
{
    glox::scoped_lock<IrqLock> _;
    return alloc_mem(size);
}

void memdealloc(void* ptr, usize size)
{
    glox::scoped_lock<IrqLock> _;
    if (!free_mem(ptr, size)) {
        // TODO: Add debuging capabilities
        gloxDebugLogln("Failed to free addr: ", ptr, " of size: ", size);
    }
}

} // namespace gx
#ifdef TEST
struct List
{
    int x, y, z;
    List* next;
};
static List* test_alloc(int sp)
{
    List* head = new List {
        .x = sp + 20,
        .y = sp + 40,
        .z = sp + 60,
        .next = nullptr,
    };
    List* iter = head;
    for (int i = 1; i < 20; ++i) {
        iter->next
            = new List { .x = i, .y = i + 20, .z = i + 40, .next = nullptr };
        iter = iter->next;
    }
    gloxPrint("List:\n");
    for (auto it = head; it; it = it->next) {
        gloxPrint(it->x, ' ');
    }
    gloxPrint("\nEndlist\n");
    return head;
}
static void test_free(List* head)
{
    for (auto it = head; it;) {
        auto tmp = it;
        it = it->next;
        gx::dealloc(tmp, 1);
    }
}
static bool test()
{
    auto tmp = test_alloc(1);
    test_free(tmp);
    tmp = test_alloc(600);
    test_free(tmp);
    return true;
}
registerTest("Allocator test", test);
#endif
