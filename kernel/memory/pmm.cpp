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
struct PmmChunk
{
	size_t size;
	friend auto operator<=>(const PmmChunk& l, const PmmChunk& r)
	{
		return &l <=> &r;
	};
};
using pmmHeader = node<PmmChunk>;
using pmmList = list<PmmChunk>;

static list<PmmChunk> pmmCtx;
static u64 memorySize;

namespace glox
{

void pmm_add_chunk(void* base, size_t length)
{
	gloxAssert(length % glox::pmmChunkSize == 0, "Pmm chunk length must be multiple of pmmChunkSize");
	const auto realBase = arch::to_virt((glox::vaddrT)base);
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
		append_chunk(pmmEnd, chunk, length);
	else if (pmmStart > chunk)
		prepend_chunk(pmmStart, chunk, length);
	else
		insert_chunk(pmmStart, chunk, length);
}

void* page_alloc(sizeT pageCount)
{
	return alloc_from_chunk(pmmCtx, glox::pmmChunkSize, pageCount);
}
void* page_alloc_z(sizeT pageCount)
{
	auto addr = page_alloc(pageCount);
	memset(addr, 0, pageCount * glox::pmmChunkSize);
	return addr;
}

void page_dealloc(void* ptr, sizeT pageCount)
{
	if (ptr == nullptr)
		return;
	glox::pmm_add_chunk(ptr, glox::pmmChunkSize * pageCount);
}
} // namespace glox

#ifdef TEST
[[maybe_unused]] static bool test()
{
	auto startSize = pmmCtx.front->size;
	void* p = page_alloc();
	void* p2 = page_alloc();
	page_dealloc(p);
	auto nextH = pmmCtx.front->next;
	KTEST_EXPECT(pmmCtx.front->next->size == 0x1000);
	page_dealloc(p2);
	KTEST_EXPECT(pmmCtx.front->size == startSize);
	KTEST_EXPECT(pmmCtx.front->next > nextH);
	return true;
}
registerTest("Page allocator", test);
#endif
