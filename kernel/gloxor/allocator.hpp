#pragma once
#include "glox/math.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#if defined(__GNUC__) && !defined(__clang__)
	#define mallocAttribute(...) malloc(__VA_ARGS__)
#else
	#define mallocAttribute(...)
#endif
namespace glox
{

	inline void free(void* ptr, sizeT size)
	{
		return glox::pmmFree(ptr, size / glox::pmmChunkSize);
	}

	[[using gnu: malloc, mallocAttribute(glox::free, 1), alloc_size(1), aligned(glox::pmmChunkSize)]] 
	inline void* alloc(sizeT bytes)
	{
		return glox::pmmAlloc(bytes/pmmChunkSize + 1);
	}

	struct kallocator
	{
		[[nodiscard]] static void* allocate(sizeT size) { return glox::alloc(size); }
		static void free(void* p, sizeT s) { glox::free(p, s); }
	};
} // namespace glox
