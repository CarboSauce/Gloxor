#pragma once
#include "glox/types.hpp"

[[nodiscard]] inline void* operator new(size_t, void* p) { return p; }
[[nodiscard]] inline void* operator new[](size_t, void* p) { return p; }

namespace glox
{
template <typename T>
void uninitDefConstruct(T first, T last)
{
	for (; first != last; first++)
		::new (first) T;
}
template <typename T>
void uninitValConstruct(T first, T last)
{
	for (; first != last; first++)
		::new (first) T();
}
template <typename T, typename Allocator>
T* alloc(Allocator a, size_t size = 1)
{
	T* ptr = (T*)(a.alloc(sizeof(T) * size));
	if (ptr == nullptr)
		return nullptr;
	for (size_t i = 0; i < size; ++i)
	{
		::new (ptr + i) T();
	}
	return ptr;
}
template <typename T, typename Allocator>
void dealloc(Allocator a, T* ptr, size_t size = 1)
{
	if (ptr == nullptr)
		return;
	for (size_t i = 0; i < size; ++i)
	{
		ptr[i].~T();
	}
	a.dealloc(ptr, sizeof(T) * size);
}

} // namespace glox