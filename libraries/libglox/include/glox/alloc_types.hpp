#pragma once
#ifndef USE_MODULES
#include <concepts>
#include <cstddef>
#endif

namespace glox {
GLOX_BEGIN_EXPORT
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
GLOX_END_EXPORT
} // namespace glox
