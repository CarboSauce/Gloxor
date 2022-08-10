#include "pmm.hpp"
#include "arch/addrspace.hpp"
#include "system/logging.hpp"
#include <glox/assert.hpp>
#include <gloxor/test.hpp>
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
	return allocFromChunk(pmmCtx, glox::pmmChunkSize, pageCount);
}
void* pageAllocZ(sizeT pageCount)
{
	auto addr = pageAlloc(pageCount);
	memset(addr, 0, pageCount * glox::pmmChunkSize);
	return addr;
}

void pageDealloc(void* ptr, sizeT pageCount)
{
	if (ptr == nullptr)
		return;
	glox::pmmAddChunk(ptr, glox::pmmChunkSize * pageCount);
}
} // namespace glox

#ifdef TEST
[[maybe_unused]] static bool test()
{
	auto startSize = pmmCtx.front->size;
	void* p = pageAlloc();
	void* p2 = pageAlloc();
	pageDealloc(p);
	auto nextH = pmmCtx.front->next;
	KTEST_EXPECT(pmmCtx.front->next->size == 0x1000);
	pageDealloc(p2);
	KTEST_EXPECT(pmmCtx.front->size == startSize);
	KTEST_EXPECT(pmmCtx.front->next > nextH);
	return true;
}
registerTest("Page allocator", test);
#endif
