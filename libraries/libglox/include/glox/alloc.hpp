#pragma once
#include "glox/assert.hpp"
#include "glox/detail/movesem.hpp"
#include <concepts>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

#ifdef LIBGLOX_DEFAULT_ALLOCATOR_PATH
#include LIBGLOX_DEFAULT_ALLOCATOR_PATH
#else
#define LIBGLOX_DEFAULT_ALLOCATOR_NAME glox::detail::default_allocator
namespace glox {
template <typename T>
struct alloc_handle
{
    T* ptr;
    std::size_t count;
};
template <typename T>
concept allocator
    = std::copy_constructible<T> and std::move_constructible<T>
  and std::destructible<T>
  and requires(
      T& allocator,
      void* ptr,
      std::size_t size,
      std::size_t alignment
  ) {
          {
              allocator.alloc(size, alignment)
          } -> std::same_as<alloc_handle<void>>;
          allocator.dealloc(ptr, size, alignment);
          {
              allocator.alloc_zeroed(size, alignment)
          } -> std::same_as<alloc_handle<void>>;
          {
              allocator.grow(ptr, size, alignment, size, alignment)
          } -> std::same_as<alloc_handle<void>>;
          {
              allocator.grow_zeroed(ptr, size, alignment, size, alignment)
          } -> std::same_as<alloc_handle<void>>;
          {
              allocator.shrink(ptr, size, alignment, size, alignment)
          } -> std::same_as<alloc_handle<void>>;
      };
namespace detail {
    struct default_allocator
    {
        alloc_handle<void> alloc(std::size_t s, std::size_t alignment)
        {
            return { std::aligned_alloc(alignment, s), s };
        }
        void dealloc(
            void* p,
            [[maybe_unused]] std::size_t s,
            [[maybe_unused]] std::size_t alignment
        )
        {
            return std::free(p);
        }
        alloc_handle<void>
        alloc_zeroed(std::size_t size, [[maybe_unused]] std::size_t alignment)
        {
            auto ptr = std::aligned_alloc(alignment, size);
            if (ptr == nullptr) {
                return { nullptr, 0 };
            }
            return { std::memset(ptr, 0, size), size };
        }

        alloc_handle<void> grow(
            void* old_ptr,
            std::size_t old_size,
            std::size_t old_alignment,
            std::size_t new_size,
            std::size_t new_alignment
        )
        {
            gloxAssert(
                old_size <= new_size,
                "New size must be greater than or equal to old size"
            );

            auto newPtr = std::aligned_alloc(new_alignment, new_size);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, old_size);
            dealloc(old_ptr, old_size, old_alignment);

            return { newPtr, new_size };
        }

        alloc_handle<void> grow_zeroed(
            void* old_ptr,
            std::size_t old_size,
            std::size_t old_alignment,
            std::size_t new_size,
            std::size_t new_alignment
        )
        {
            gloxAssert(
                old_size <= new_size,
                "New size must be greater than or equal to old size"
            );

            auto newPtr = std::aligned_alloc(new_alignment, new_size);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, old_size);
            std::memset(
                reinterpret_cast<void*>(
                    reinterpret_cast<uintptr_t>(newPtr) + old_size
                ),
                0,
                new_size - old_size
            );
            dealloc(old_ptr, old_size, old_alignment);

            return { newPtr, new_size };
        }

        alloc_handle<void> shrink(
            void* old_ptr,
            std::size_t old_size,
            std::size_t old_alignment,
            std::size_t new_size,
            std::size_t new_alignment
        )
        {
            gloxAssert(
                old_size >= new_size,
                "New size must be smaller than or equal to old size"
            );

            auto newPtr = std::aligned_alloc(new_alignment, new_size);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, new_size);
            dealloc(old_ptr, old_size, old_alignment);

            return { newPtr, new_size };
        }
    };
} // namespace detail
} // namespace glox
#endif

static_assert(
    glox::allocator<LIBGLOX_DEFAULT_ALLOCATOR_NAME>,
    "Default allocator satisfies allocator"
);
namespace glox {
using default_allocator = LIBGLOX_DEFAULT_ALLOCATOR_NAME;
}; // namespace glox

namespace glox {
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

template <typename T, glox::allocator Allocator>
T* alloc(Allocator a, size_t size = 1, size_t alignment = alignof(T))
{
    T* ptr = (T*)(a.alloc(sizeof(T) * size, alignment).ptr);
    if (ptr == nullptr)
        return nullptr;
    for (size_t i = 0; i < size; ++i) {
        ::new (ptr + i) T();
    }
    return ptr;
}

template <typename T, glox::allocator Allocator>
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
    auto [ptr, size] = allocator.alloc(sizeof(T) * count, alignment);
    return { (T*)ptr, count };
}

template <typename T, glox::allocator Allocator>
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
    allocator.dealloc((void*)ptr, sizeof(T) * count, alignment);
}

template <typename T, glox::allocator Allocator>
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
    auto [ptr, size] = allocator.alloc(sizeof(T) * count, alignment);
    uninit_val_construct((T*)ptr, count);
    return { (T*)ptr, count };
}

template <typename T, glox::allocator Allocator>
constexpr alloc_handle<T> grow_alloc(
    Allocator& allocator,
    T* old_ptr,
    std::size_t old_count,
    std::size_t old_alignment,
    std::size_t new_count,
    std::size_t new_alignment
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(new_count);
        for (auto i = 0z; i != old_count; ++i) {
            std::construct_at(res.ptr + i, RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        std::allocator<T> { }.deallocate(old_ptr, old_count);

        return { res.ptr, res.count };
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        auto mem = allocator.grow(
            old_ptr,
            sizeof(T) * old_count,
            old_alignment,
            sizeof(T) * new_count,
            new_alignment
        );

        return { (T*)mem.ptr, new_count };
    } else {
        auto mem = allocator.alloc(sizeof(T) * new_count, new_alignment);
        for (std::size_t i = 0; i != old_count; ++i) {
            ::new ((T*)mem.ptr + i) T(RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        allocator.dealloc(old_ptr, sizeof(T) * old_count, old_alignment);
        return { (T*)mem.ptr, new_count };
    }
}

template <typename T, glox::allocator Allocator>
constexpr alloc_handle<T> shrink_alloc(
    Allocator& allocator,
    T* old_ptr,
    std::size_t old_count,
    std::size_t old_alignment,
    std::size_t new_count,
    std::size_t new_alignment
)
{
    if (std::is_constant_evaluated()) {
        auto res = std::allocator<T> { }.allocate_at_least(new_count);
        for (auto i = 0z; i != new_count; ++i) {
            ::new (res.ptr + i) T(RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        std::allocator<T> { }.deallocate(old_ptr, old_count);

        return { res.ptr, res.count };
    }
    if constexpr (std::is_trivially_copyable_v<T>) {
        auto mem = allocator.shrink(
            old_ptr,
            sizeof(T) * old_count,
            old_alignment,
            sizeof(T) * new_count,
            new_alignment
        );

        return { (T*)mem.ptr, new_count };
    } else {
        auto mem = allocator.alloc(sizeof(T) * new_count, new_alignment);
        for (std::size_t i = 0; i != new_count; ++i) {
            ::new ((T*)mem.ptr + i) T(RVALUE(old_ptr[i]));
        }
        for (std::size_t i = 0; i != old_count; ++i) {
            old_ptr[i].~T();
        }
        allocator.dealloc(old_ptr, sizeof(T) * old_count, old_alignment);
        return { (T*)mem.ptr, new_count };
    }
}
}; // namespace glox
