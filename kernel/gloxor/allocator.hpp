#pragma once
#include "glox/math.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"

namespace glox
{

	void free(void* ptr, sizeT size)
	{
		return glox::pmmFree(ptr, size / glox::pmmChunkSize);
	}

	[[using gnu: malloc, malloc(glox::free, 1), alloc_size(1), aligned(glox::pmmChunkSize)]] void* alloc(sizeT bytes)
	{
		return glox::pmmAlloc(ALLIGNUP(bytes, glox::pmmChunkSize));
	}

	struct kallocator
	{
		[[nodiscard]] static void* allocate(sizeT size) { return glox::alloc(size); }
		static void free(void* p, sizeT s) { glox::free(p, s); }
	};
} // namespace glox