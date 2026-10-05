#pragma once
#include "glox/alloc_types.hpp"
#include "glox/assert.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include <string.h>

namespace gx {

void memdealloc(void* ptr, usize size);

[[using gnu: malloc,
    mallocAttribute(gx::memdealloc, 1),
    alloc_size(1),
    aligned(gx::pmmChunkSize)]] void*
memalloc(usize bytes);

template <typename T>
struct KAllocator
{
    glox::alloc_handle<T>
    alloc(std::size_t count, [[maybe_unused]] std::size_t alignment)
    {
        return { (T*)gx::memalloc(count * sizeof(T)), count };
    }
    void dealloc(
        T* p,
        [[maybe_unused]] std::size_t count,
        [[maybe_unused]] std::size_t alignment
    )
    {
        return gx::memdealloc(static_cast<void*>(p), sizeof(T) * count);
    }
    glox::alloc_handle<T>
    alloc_zeroed(std::size_t count, [[maybe_unused]] std::size_t alignment)
    {
        auto ptr = (T*)gx::memalloc(sizeof(T) * count);
        if (ptr == nullptr) {
            return { nullptr, 0 };
        }
        return { ::memset(ptr, 0, sizeof(T) * count), count };
    }

    glox::alloc_handle<T> grow(
        T* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t alignment
    )
    {
        gloxAssert(
            old_count <= new_count,
            "New size must be greater than or equal to old size"
        );

        auto newPtr = (T*)gx::memalloc(sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
    }

    glox::alloc_handle<T> grow_zeroed(
        T* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t alignment
    )
    {
        gloxAssert(
            old_count <= new_count,
            "New size must be greater than or equal to old size"
        );

        auto newPtr = (T*)gx::memalloc(sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
        ::memset(newPtr + old_count, 0, sizeof(T) * (new_count - old_count));
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
    }

    glox::alloc_handle<T> shrink(
        T* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t alignment
    )
    {
        gloxAssert(
            old_count >= new_count,
            "New size must be smaller than or equal to old size"
        );

        auto newPtr = gx::memalloc(sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, new_count);
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
    }
    glox::alloc_handle<T> grow_inplace(
        [[maybe_unused]] T* old_ptr,
        [[maybe_unused]] std::size_t old_size,
        [[maybe_unused]] std::size_t new_size,
        [[maybe_unused]] std::size_t alignment
    )
    {
        // in std there isn't really a way to implement this
        return { nullptr, 0 };
    }

    glox::alloc_handle<T> shrink_inplace(
        [[maybe_unused]] T* old_ptr,
        [[maybe_unused]] std::size_t old_count,
        [[maybe_unused]] std::size_t new_count,
        [[maybe_unused]] std::size_t alignment
    )
    {
        // in std there isn't really a way to implement this
        return { nullptr, 0 };
    }
};

struct PmmAllocator
{
    [[gnu::always_inline]] [[nodiscard]] static void* alloc(usize s)
    {
        return gx::page_alloc(s / pmmChunkSize + 1);
    }
    [[gnu::always_inline]] static void dealloc(void* p, usize s)
    {
        gx::page_dealloc(p, s / pmmChunkSize + 1);
    }
};

template <typename T>
T* alloc(size_t ele_count = 1)
{
    T* ptr = (T*)(memalloc(sizeof(T) * ele_count));
    if (ptr == nullptr)
        return nullptr;
    for (size_t i = 0; i < ele_count; ++i) {
        ::new (ptr + i) T();
    }
    return ptr;
}
template <typename T>
void dealloc(T* ptr, size_t ele_count)
{
    if (ptr == nullptr)
        return;
    for (size_t i = 0; i < ele_count; ++i) {
        ptr[i].~T();
    }
    memdealloc(ptr, sizeof(T) * ele_count);
}

} // namespace gx

namespace glox {
template <typename T>
using default_allocator = gx::KAllocator<T>;
} // namespace glox
