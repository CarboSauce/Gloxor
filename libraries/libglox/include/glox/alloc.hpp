#pragma once
#include "glox/assert.hpp"
#include "glox/detail/memory.hpp"
#include "glox/detail/movesem.hpp"
#include <concepts>
#include <cstdlib>
#include <cstring>
#include <type_traits>

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
template <typename Alloc, typename T>
concept allocator
    = std::copy_constructible<Alloc> and std::move_constructible<Alloc>
  and std::destructible<Alloc>
  and requires(
      Alloc& allocator,
      T* ptr,
      std::size_t count,
      std::size_t alignment
  ) {
          {
              allocator.alloc(count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          { allocator.dealloc(ptr, count, alignment) } -> std::same_as<void>;
          {
              allocator.alloc_zeroed(count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          {
              allocator.grow(ptr, count, count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          {
              allocator.grow_inplace(ptr, count, count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          {
              allocator.grow_zeroed(ptr, count, count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          {
              allocator.shrink(ptr, count, count, alignment)
          } -> std::same_as<alloc_handle<T>>;
          {
              allocator.shrink_inplace(ptr, count, count, alignment)
          } -> std::same_as<alloc_handle<T>>;
      };
namespace detail {
    template <typename T>
    struct default_allocator
    {
        alloc_handle<T> alloc(std::size_t count, std::size_t alignment)
        {
            return { (T*)std::aligned_alloc(alignment, count * sizeof(T)),
                count };
        }
        void dealloc(
            T* p,
            [[maybe_unused]] std::size_t count,
            [[maybe_unused]] std::size_t alignment
        )
        {
            return std::free(static_cast<void*>(p));
        }
        alloc_handle<T>
        alloc_zeroed(std::size_t count, [[maybe_unused]] std::size_t alignment)
        {
            auto ptr = (T*)std::aligned_alloc(alignment, sizeof(T) * count);
            if (ptr == nullptr) {
                return { nullptr, 0 };
            }
            return { std::memset(ptr, 0, sizeof(T) * count), count };
        }

        alloc_handle<T> grow(
            T* old_ptr,
            std::size_t old_count,
            std::size_t new_count,
            std::size_t alignment
        )
        {
            gloxAssert(
                old_size <= new_size,
                "New size must be greater than or equal to old size"
            );

            auto newPtr
                = (T*)std::aligned_alloc(alignment, sizeof(T) * new_count);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
            dealloc(old_ptr, old_count, alignment);

            return { newPtr, new_count };
        }

        alloc_handle<T> grow_zeroed(
            T* old_ptr,
            std::size_t old_count,
            std::size_t new_count,
            std::size_t alignment
        )
        {
            gloxAssert(
                old_size <= new_size,
                "New size must be greater than or equal to old size"
            );

            auto newPtr
                = (T*)std::aligned_alloc(alignment, sizeof(T) * new_count);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, sizeof(T) * old_count);
            std::memset(
                newPtr + old_count, 0, sizeof(T) * (new_count - old_count)
            );
            dealloc(old_ptr, old_count, alignment);

            return { newPtr, new_count };
        }

        alloc_handle<T> shrink(
            T* old_ptr,
            std::size_t old_count,
            std::size_t new_count,
            std::size_t alignment
        )
        {
            gloxAssert(
                old_size >= new_size,
                "New size must be smaller than or equal to old size"
            );

            auto newPtr = std::aligned_alloc(alignment, new_count);
            if (newPtr == nullptr) {
                return { nullptr, 0 };
            }

            std::memcpy(newPtr, old_ptr, new_count);
            dealloc(old_ptr, old_count, alignment);

            return { newPtr, new_count };
        }

        alloc_handle<T> grow_inplace(
            [[maybe_unused]] T* old_ptr,
            [[maybe_unused]] std::size_t old_size,
            [[maybe_unused]] std::size_t new_size,
            [[maybe_unused]] std::size_t alignment
        )
        {
            // in std there isn't really a way to implement this
            return { nullptr, 0 };
        }

        alloc_handle<T> shrink_inplace(
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
} // namespace detail
} // namespace glox
#endif

namespace glox {
template <typename T>
using default_allocator = LIBGLOX_DEFAULT_ALLOCATOR_NAME<T>;
}; // namespace glox
static_assert(
    glox::allocator<glox::default_allocator<int>, int>,
    "Default allocator satisfies allocator"
);

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

template <typename T, glox::allocator<T> Allocator>
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
}; // namespace glox
