#pragma once
#include "glox/alloc.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"

namespace gx
{

void memdealloc(void* ptr, sizeT size);

[[using gnu: malloc, mallocAttribute(gx::memdealloc, 1), alloc_size(1), aligned(gx::pmmChunkSize)]] void* memalloc(sizeT bytes);

struct KAllocator
{
	[[gnu::always_inline]] [[nodiscard]] static void* alloc(sizeT size) { return gx::memalloc(size); }
	[[gnu::always_inline]] static void dealloc(void* p, sizeT s) { gx::memdealloc(p, s); }
};
using default_allocator = KAllocator;

struct PmmAllocator
{
	[[gnu::always_inline]] [[nodiscard]] static void* alloc(sizeT s) { return gx::page_alloc(s / pmmChunkSize + 1); }
	[[gnu::always_inline]] static void dealloc(void* p, sizeT s) { gx::page_dealloc(p, s / pmmChunkSize + 1); }
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
void dealloc(T* ptr, size_t size)
{
	if (ptr == nullptr)
		return;
	for (size_t i = 0; i < size; ++i)
	{
		ptr[i].~T();
	}
	memdealloc(ptr, sizeof(T) * size);
}

} // namespace gx
