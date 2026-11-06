module gloxor.memory.alloc;
import gloxor.memory;
import glox;
import gloxor.system.logging;
import gloxor.mutex;
import gloxor.types;
import gloxor.memory.pmm;
import std;
import std.compat;

#include "glox/assert.hpp"
#include "system/logging.hpp"

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
namespace gx {
void* memalloc(usize size);
void memdealloc(void* ptr, usize size);
void memdealloc(void* ptr);
} // namespace gx

extern "C++"
{
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

void operator delete(void* p)
{
    gx::memdealloc(p);
}

void operator delete[](void* p, size_t size)
{
    gx::memdealloc(p, size);
}

void operator delete[](void* p)
{
    gx::memdealloc(p);
}
}

namespace glox::custom {

glox::alloc_handle<void> alloc(
    std::size_t count,
    std::size_t objsize,
    [[maybe_unused]] std::size_t alignment
)
{
    return { gx::memalloc(count * objsize), count };
}
void dealloc(
    void* p,
    [[maybe_unused]] std::size_t count,
    std::size_t objsize,
    [[maybe_unused]] std::size_t alignment
)
{
    return gx::memdealloc(static_cast<void*>(p), objsize * count);
}
glox::alloc_handle<void> alloc_zeroed(
    std::size_t count,
    std::size_t objsize,
    [[maybe_unused]] std::size_t alignment
)
{
    auto ptr = (void*)gx::memalloc(objsize * count);
    if (ptr == nullptr) {
        return { nullptr, 0 };
    }
    return { glox::memset(ptr, 0, objsize * count), count };
}

glox::alloc_handle<void> grow(
    void* old_ptr,
    std::size_t old_count,
    std::size_t new_count,
    std::size_t objsize,
    std::size_t alignment
)
{
    GLOX_ASSERT(
        old_count <= new_count,
        "New size must be greater than or equal to old size"
    );

    auto newPtr = (void*)gx::memalloc(objsize * new_count);
    if (newPtr == nullptr) {
        return { nullptr, 0 };
    }

    glox::memcpy(newPtr, old_ptr, objsize * old_count);
    glox::custom::dealloc(old_ptr, old_count, objsize, alignment);

    return { newPtr, new_count };
}

glox::alloc_handle<void> grow_zeroed(
    void* old_ptr,
    std::size_t old_count,
    std::size_t new_count,
    std::size_t objsize,
    std::size_t alignment
)
{
    GLOX_ASSERT(
        old_count <= new_count,
        "New size must be greater than or equal to old size"
    );

    auto newPtr = (void*)gx::memalloc(objsize * new_count);
    if (newPtr == nullptr) {
        return { nullptr, 0 };
    }

    glox::memcpy(newPtr, old_ptr, objsize * old_count);
    glox::memset(
        (u8*)newPtr + objsize * old_count, 0, objsize * (new_count - old_count)
    );
    glox::custom::dealloc(old_ptr, old_count, objsize, alignment);

    return { newPtr, new_count };
}

glox::alloc_handle<void> shrink(
    void* old_ptr,
    std::size_t old_count,
    std::size_t new_count,
    std::size_t objsize,
    std::size_t alignment
)
{
    GLOX_ASSERT(
        old_count >= new_count,
        "New size must be smaller than or equal to old size"
    );

    auto newPtr = gx::memalloc(objsize * new_count);
    if (newPtr == nullptr) {
        return { nullptr, 0 };
    }

    glox::memcpy(newPtr, old_ptr, new_count);
    glox::custom::dealloc(old_ptr, old_count, objsize, alignment);

    return { newPtr, new_count };
}
glox::alloc_handle<void> grow_inplace(
    [[maybe_unused]] void* old_ptr,
    [[maybe_unused]] std::size_t old_size,
    [[maybe_unused]] std::size_t new_size,
    [[maybe_unused]] std::size_t objsize,
    [[maybe_unused]] std::size_t alignment
)
{
    // in std there isn't really a way to implement this
    return { nullptr, 0 };
}

glox::alloc_handle<void> shrink_inplace(
    [[maybe_unused]] void* old_ptr,
    [[maybe_unused]] std::size_t old_count,
    [[maybe_unused]] std::size_t new_count,
    [[maybe_unused]] std::size_t objsize,
    [[maybe_unused]] std::size_t alignment
)
{
    // in std there isn't really a way to implement this
    return { nullptr, 0 };
}
} // namespace glox::custom

namespace gx {

struct Palloc
{
    glox::alloc_handle<void> alloc(size_t s)
    {
        return { PmmAllocator::alloc(s), s / 4096 + 1 };
    }

    void dealloc(void* p, size_t s)
    {
        return PmmAllocator::dealloc(p, s);
    }

    glox::alloc_handle<void> grow(void* p, size_t oldsize, size_t newsize)
    {
        return { nullptr, 0 };
    }
    glox::alloc_handle<void> shrink(void* p, size_t oldsize, size_t newsize)
    {
        return { nullptr, 0 };
    }
};

static constexpr std::size_t PAGE_SIZE = 65536;
static constexpr std::size_t MAX_SMALL_SIZE = 2048;
static constexpr std::size_t GRAUNULARITY = 8;
static constexpr std::size_t NUM_BUCKETS = MAX_SMALL_SIZE / GRAUNULARITY;

struct ThreadRoutines
{
    size_t thread_id()
    {
        return 0;
    }

    auto& get_tcache()
    {
        return tcache;
    }
    static inline glox::thread_cache<NUM_BUCKETS> tcache { };
};

static glox::heap_allocator<Palloc, ThreadRoutines> globalHeap;

void* memalloc(usize size)
{
    ::glox::scoped_lock<IrqLock> _;
    return globalHeap.alloc(size).ptr;
}

void memdealloc(void* ptr, usize size)
{
    ::glox::scoped_lock<IrqLock> _;
    return globalHeap.dealloc(ptr, size);
}

void memdealloc(void* ptr)
{
    ::glox::scoped_lock<IrqLock> _;
    return globalHeap.dealloc(ptr);
}

} // namespace gx
