#include "pmm.hpp"
#include "arch/addrspace.hpp"
#include "system/logging.hpp"
#include <glox/assert.hpp>
#include <gloxor/modules.hpp>
#include <memory/llallocator.hpp>

using namespace glox;

/*
	@TODO: size could perhaps be replaced pointer to end of range
*/
struct pmmChunk
{
	size_t size;
	friend auto operator<=>(const pmmChunk& l, const pmmChunk& r)
	{
		return &l <=> &r;
	};
};
using pmmHeader = node<pmmChunk>;
using pmmList = list<pmmChunk>;

static list<pmmChunk> pmmCtx;
static u64 memorySize;

namespace glox
{

void pmmAddChunk(void* base, size_t length)
{
	gloxAssert(length % glox::pmmChunkSize == 0, "Pmm chunk length must be multiple of pmmChunkSize");
	const auto realBase = arch::toVirt((glox::vaddrT)base);
	auto* chunk = reinterpret_cast<pmmHeader*>(realBase);
	auto& pmmStart = pmmCtx.front;
	auto& pmmEnd = pmmCtx.back;
	chunk->next = nullptr;
	chunk->size = length;
	memorySize += length;
	if (pmmStart == nullptr)
	{
		pmmStart = chunk;
		pmmStart->prev = nullptr;
		pmmEnd = chunk;
	}
	else if (pmmEnd < chunk)
		appendChunk(pmmEnd, chunk, length);
	else if (pmmStart > chunk)
		prependChunk(pmmStart, chunk, length);
	else
		insertChunk(pmmStart, chunk, length);
}

void* pageAlloc(sizeT pageCount)
{
	auto mem = allocFromChunk(pmmCtx, glox::pmmChunkSize, pageCount);
	return mem;
}
void* pageAllocZ(sizeT pageCount)
{
	auto addr = pageAlloc(pageCount);
	memset(addr, 0, pageCount * glox::pmmChunkSize);
	return addr;
}

// void pmmFree(void* ptr)
// {
// 	glox::pmmAddChunk(ptr, glox::pmmChunkSize);
// }
void pageDealloc(void* ptr, sizeT pageCount)
{
	if (ptr == nullptr)
		return;
	glox::pmmAddChunk(ptr, glox::pmmChunkSize * pageCount);
}
} // namespace glox

[[maybe_unused]] static void test()
{
	gloxDebugLog("Memory size: ", memorySize, " pmm test\nBefore:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.size);
	}

	constexpr auto size = 200;
	static glox::array<void*, size> ptrs;
	for (int i = 0; i < size; ++i)
		ptrs[i] = pageAlloc();
	gloxDebugLog("\nAfter allocations:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.size);
	}
	for (int i = 0; i < size; i++)
		pageDealloc(ptrs[i]);

	gloxDebugLog("\nAfter freeing:\n");
	for (const auto& it : pmmCtx)
	{
		gloxDebugLogln(&it, '-', (char*)&it + it.size);
	}
}

//registerTest(test);
