module gloxor.memory.alloc;
import gloxor.memory;
import glox;
import gloxor.system.logging;
import gloxor.mutex;
import gloxor.types;
import gloxor.memory.pmm;
import gloxor.system.danger;
import gloxor.system.percpu;
import arch.percpu;
import std;
import std.compat;

#include "glox/assert.hpp"

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
            (u8*)newPtr + objsize * old_count,
            0,
            objsize * (new_count - old_count)
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
}

namespace gx {

struct Palloc
{
    glox::alloc_handle<void> alloc(size_t s)
    {
        return BuddyAllocator::alloc(s);
    }

    void dealloc(void* p, size_t s)
    {
        return BuddyAllocator::dealloc(p, s);
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

static constexpr std::size_t MAX_SMALL_SIZE = 2048;
static constexpr std::size_t MAX_MEDIUM_SIZE = 65536;
static constexpr std::size_t GRANULARITY = 8;
static constexpr std::size_t NUM_BUCKETS = glox::heap_detail::
    total_bucket_count<MAX_SMALL_SIZE, GRANULARITY, MAX_MEDIUM_SIZE>();

struct ThreadRoutines
{
    size_t thread_id()
    {
        return 0;
    }

    auto& get_tcache()
    {
        return *tcache.ptr();
    }
    DEFINE_PERCPU constinit static inline arch::Percpu<
        glox::kthread_cache<NUM_BUCKETS>
    >
        tcache { };
};

struct Policy
{
    static constexpr bool enabled = true;
    static constexpr bool deep = false;
    static void fail()
    {
        GLOX_ASSERT("Failure in kheap allocator");
    }
};

using global_heap_alloc = glox::kheap_allocator<
    Palloc,
    PfndbAccessor,
    ThreadRoutines,
    glox::spinlock,
    buddyAllocator.PAGE_SHIFT,
    MAX_SMALL_SIZE,
    GRANULARITY,
    MAX_MEDIUM_SIZE,
    Policy
>;

static global_heap_alloc globalHeap;

static constexpr bool TRACK_MEMALLOC = true;

void* memalloc(usize size)
{
    gx::IrqScopeLock _;
    if constexpr (TRACK_MEMALLOC) {
        gx::println("[memalloc] Allocating with size %", size);
    }
    return globalHeap.alloc(size).ptr;
}

void memdealloc(void* ptr, usize size)
{
    gx::IrqScopeLock _;
    if constexpr (TRACK_MEMALLOC) {
        gx::println("[memalloc] Deallocating %x with size %", ptr, size);
    }
    return globalHeap.dealloc(ptr, size);
}

void memdealloc(void* ptr)
{
    gx::IrqScopeLock _;
    if constexpr (TRACK_MEMALLOC) {
        gx::println("Deallocating %x ", ptr);
    }
    return globalHeap.dealloc(ptr);
}

} // namespace gx
