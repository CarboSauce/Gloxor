#pragma once
#include "glox/alloc.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"

namespace glox
{

inline void memdealloc(void* ptr, sizeT size)
{
	return glox::pageDealloc(ptr, size / glox::pmmChunkSize + 1);
}

[[using gnu: malloc, mallocAttribute(glox::memdealloc, 1), alloc_size(1), aligned(glox::pmmChunkSize)]] inline void* memalloc(sizeT bytes)
{
	return glox::pageAlloc(bytes / pmmChunkSize + 1);
}

struct kAllocator
{
	[[gnu::always_inline]] [[nodiscard]] static void* alloc(sizeT size) { return glox::memalloc(size); }
	[[gnu::always_inline]] static void dealloc(void* p, sizeT s) { glox::memdealloc(p, s); }
};

struct pmmAllocator
{
	[[gnu::always_inline]] [[nodiscard]] static void* alloc(sizeT s) { return glox::pageAlloc(s / pmmChunkSize + 1); }
	[[gnu::always_inline]] static void dealloc(void* p, sizeT s) { glox::pageDealloc(p, s / pmmChunkSize + 1); }
};

template <typename T>
T* alloc(size_t size = 1)
{
	T* ptr = (T*)(memalloc(sizeof(T) * size));
	if (ptr == nullptr)
		return nullptr;
	for (size_t i = 0; i < size; ++i)
	{
		::new (ptr + i) T();
	}
	return ptr;
}
template <typename T>
void dealloc(T* ptr, size_t size = 1)
{
	if (ptr == nullptr)
		return;
	for (size_t i = 0; i < size; ++i)
	{
		ptr[i].~T();
	}
	memdealloc(ptr, sizeof(T) * size);
}

} // namespace glox
