#pragma once
#include "glox/assert.hpp"

#ifndef USE_MODULES
#include "alloc_types.hpp"
#include "glox/detail/memory.hpp"
#include "glox/detail/movesem.hpp"
#include <cstdlib>
#include <string.h>
#include <type_traits>
#endif

namespace glox {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
namespace custom {
    alloc_handle<void>
    alloc(std::size_t count, std::size_t objsize, std::size_t alignment);
    void dealloc(
        void* p,
        std::size_t count,
        std::size_t objsize,
        std::size_t alignment
    );
    alloc_handle<void>
    alloc_zeroed(std::size_t count, std::size_t objsize, std::size_t alignment);

    alloc_handle<void> grow(
        void* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t objsize,
        std::size_t alignment
    );

    alloc_handle<void> grow_zeroed(
        void* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t objsize,
        std::size_t alignment
    );

    alloc_handle<void> shrink(
        void* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t objsize,
        std::size_t alignment
    );

    alloc_handle<void> grow_inplace(
        void* old_ptr,
        std::size_t old_size,
        std::size_t new_size,
        std::size_t objsize,
        std::size_t alignment
    );

    alloc_handle<void> shrink_inplace(
        void* old_ptr,
        std::size_t old_count,
        std::size_t new_count,
        std::size_t objsize,
        std::size_t alignment
    );

} // namespace custom
#endif

GLOX_EXPORT template <typename T>
struct default_allocator
{
    alloc_handle<T> alloc(std::size_t count, std::size_t alignment)
    {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::alloc(count, sizeof(T), alignment);
        return { (T*)mem.ptr, mem.count };
#else
        return { (T*)std::aligned_alloc(alignment, count * sizeof(T)), count };
#endif
    }
    void dealloc(
        T* p,
        [[maybe_unused]] std::size_t count,
        [[maybe_unused]] std::size_t alignment
    )
    {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        return glox::custom::dealloc(p, count, sizeof(T), alignment);
#else
        return std::free(static_cast<void*>(p));
#endif
    }
    alloc_handle<T>
    alloc_zeroed(std::size_t count, [[maybe_unused]] std::size_t alignment)
    {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::alloc_zeroed(count, sizeof(T), alignment);
        return { (T*)mem.ptr, mem.count };
#else
        auto ptr = (T*)std::aligned_alloc(alignment, sizeof(T) * count);
        if (ptr == nullptr) {
            return { nullptr, 0 };
        }
        return { memset(ptr, 0, sizeof(T) * count), count };
#endif
    }

    alloc_handle<T> grow(
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
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::grow(
            old_ptr, old_count, new_count, sizeof(T), alignment
        );
        return { (T*)mem.ptr, mem.count };
#else
        auto newPtr = (T*)aligned_alloc(alignment, sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
#endif
    }

    alloc_handle<T> grow_zeroed(
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

#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::grow_zeroed(
            old_ptr, old_count, new_count, sizeof(T), alignment
        );
        return { (T*)mem.ptr, mem.count };
#else
        auto newPtr = (T*)std::aligned_alloc(alignment, sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
        ::memset(newPtr + old_count, 0, sizeof(T) * (new_count - old_count));
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
#endif
    }

    alloc_handle<T> shrink(
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

#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::shrink(
            old_ptr, old_count, new_count, sizeof(T), alignment
        );
        return { (T*)mem.ptr, mem.count };
#else
        auto newPtr = std::aligned_alloc(alignment, sizeof(T) * new_count);
        if (newPtr == nullptr) {
            return { nullptr, 0 };
        }

        ::memcpy(newPtr, old_ptr, new_count);
        dealloc(old_ptr, old_count, alignment);

        return { newPtr, new_count };
#endif
    }

    alloc_handle<T> grow_inplace(
        [[maybe_unused]] T* old_ptr,
        [[maybe_unused]] std::size_t old_count,
        [[maybe_unused]] std::size_t new_count,
        [[maybe_unused]] std::size_t alignment
    )
    {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::grow_inplace(
            old_ptr, old_count, new_count, sizeof(T), alignment
        );
        return { (T*)mem.ptr, mem.count };
#else
        // in std there isn't really a way to implement this
        return { nullptr, 0 };
#endif
    }

    alloc_handle<T> shrink_inplace(
        [[maybe_unused]] T* old_ptr,
        [[maybe_unused]] std::size_t old_count,
        [[maybe_unused]] std::size_t new_count,
        [[maybe_unused]] std::size_t alignment
    )
    {
#ifdef CUSTOM_DEFAULT_ALLOCATOR
        auto mem = glox::custom::shrink_inplace(
            old_ptr, old_count, new_count, sizeof(T), alignment
        );
        return { (T*)mem.ptr, mem.count };
#else
        // in std there isn't really a way to implement this
        return { nullptr, 0 };
#endif
    }
};
} // namespace glox

static_assert(
    glox::allocator<glox::default_allocator<int>, int>,
    "Default allocator satisfies allocator"
);

namespace glox {
GLOX_BEGIN_EXPORT
template <typename T>
    requires std::is_pointer_v<T>
void uninit_def_construct(T first, T last)
{
    for (; first != last; first++)
        ::new (first) T;
}

template <typename T>
    requires std::is_pointer_v<T>
void uninit_def_construct(T first, std::size_t count)
{
    for (std::size_t i = 0; i != count; i++)
        ::new (first + i) T;
}

template <typename T>
    requires std::is_pointer_v<T>
void uninit_val_construct(T first, T last)
{
    for (; first != last; first++)
        ::new (first) T();
}

template <typename T>
    requires std::is_pointer_v<T>
void uninit_val_construct(T first, std::size_t count)
{
    for (std::size_t i = 0; i != count; i++)
        ::new (first + i) T();
}

template <typename T, glox::allocator<T> Allocator>
T* alloc(Allocator& a, size_t size = 1, size_t alignment = alignof(T))
{
    T* ptr = (T*)(a.alloc(sizeof(T) * size, alignment).ptr);
    if (ptr == nullptr)
        return nullptr;
    for (size_t i = 0; i < size; ++i) {
        ::new (ptr + i) T();
    }
    return ptr;
}

template <typename T, glox::allocator<T> Allocator>
constexpr alloc_handle<T> alloc_uninitialized(
    Allocator& allocator,
    std::size_t count,
    std::size_t alignment = alignof(T)
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(count);
        return { res.ptr, res.count };
    }
    return allocator.alloc(count, alignment);
}

template <typename T, glox::allocator<T> Allocator>
constexpr void dealloc(
    Allocator& allocator,
    T* ptr,
    std::size_t count,
    std::size_t alignment = alignof(T)
)
{
    if (std::is_constant_evaluated()) {
        std::allocator<T> { }.deallocate(ptr, count);
        return;
    }
    allocator.dealloc(ptr, count, alignment);
}

template <typename T, glox::allocator<T> Allocator>
constexpr alloc_handle<T> alloc_value_initialized(
    Allocator& allocator,
    std::size_t count,
    std::size_t alignment = alignof(T)
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(count);
        return { res.ptr, res.count };
    }
    auto ptr = allocator.alloc(count, alignment);
    uninit_val_construct((T*)ptr, count);
    return ptr;
}

template <typename T, glox::allocator<T> Allocator>
constexpr alloc_handle<T> grow_alloc(
    Allocator& allocator,
    T* old_ptr,
    std::size_t old_count,
    std::size_t new_count,
    std::size_t old_constructed_count,
    std::size_t alignment = alignof(T)
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(new_count);
        for (auto i = 0uz; i != old_count; ++i) {
            std::construct_at(res.ptr + i, RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        std::allocator<T> { }.deallocate(old_ptr, old_count);

        return { res.ptr, res.count };
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        return allocator.grow(old_ptr, old_count, new_count, alignment);
    } else {
        alloc_handle<T> mem
            = allocator.grow_inplace(old_ptr, old_count, new_count, alignment);
        if (mem.ptr != nullptr) {
            return mem;
        }
        mem = allocator.alloc(new_count, alignment);
        for (std::size_t i = 0; i != old_constructed_count; ++i) {
            ::new ((T*)mem.ptr + i) T(RVALUE(old_ptr[i]));
        }
        gloxAssert(old_constructed_count <= old_count);
        for (std::size_t i = 0; i != old_constructed_count; ++i) {
            old_ptr[i].~T();
        }
        allocator.dealloc(old_ptr, old_count, alignment);
        return mem;
    }
}

template <typename T, glox::allocator<T> Allocator>
constexpr alloc_handle<T> shrink_alloc(
    Allocator& allocator,
    T* old_ptr,
    std::size_t old_count,
    std::size_t new_count,
    std::size_t old_constructed_count,
    std::size_t alignment = alignof(T)
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(new_count);
        for (auto i = 0uz; i != new_count; ++i) {
            ::new (res.ptr + i) T(RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        std::allocator<T> { }.deallocate(old_ptr, old_count);

        return { res.ptr, res.count };
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        return allocator.shrink(old_ptr, old_count, new_count, alignment);
    } else {
        alloc_handle<T> mem = allocator.shrink_inplace(
            old_ptr, old_count, new_count, alignment
        );
        if (mem.ptr != nullptr) {
            return mem;
        }
        mem = allocator.alloc(new_count, alignment);
        for (std::size_t i = 0; i != new_count; ++i) {
            ::new ((T*)mem.ptr + i) T(RVALUE(old_ptr[i]));
        }
        gloxAssert(old_constructed_count <= old_count);
        for (std::size_t i = 0; i != old_constructed_count; ++i) {
            old_ptr[i].~T();
        }
        allocator.dealloc(old_ptr, old_count, alignment);
        return mem;
    }
}
GLOX_END_EXPORT
}; // namespace glox
