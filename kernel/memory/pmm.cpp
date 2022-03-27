#include "pmm.hpp"
#include "arch/paging.hpp"
#include "arch/segments.hpp"
#include "system/logging.hpp"
#include <bit>
#include <glox/assert.hpp>
#include <glox/bitfields.hpp>
#include <gloxor/modules.hpp>
#include <glox/util.hpp>

namespace glox
{

	// This should point to context initialized by early protocol inits
	glox::pmmHeader* pmmStart;
	glox::pmmHeader* pmmEnd;
	glox::pmmHeader* pmmCtx;
	void pmmAddChunk(void* base, size_t length)
	{
		auto* chunk = (glox::pmmHeader*)base;
		pmmEnd = chunk;
		chunk->next = nullptr;
		chunk->size = length;
		if (pmmCtx == nullptr)
		{
			pmmCtx = chunk;
			pmmStart = chunk;
			pmmCtx->prev = nullptr;
		}
		else
		{
			chunk->prev = pmmCtx;
			pmmCtx->next = chunk;
			pmmCtx = chunk;
		}
	}

	void pmmFinalize()
	{
		pmmCtx = pmmStart;
	}

	void* pmmAlloc()
	{
		for (const auto& i : *pmmStart)
		{
			gloxDebugLogln("Meme allocator: ", &i,'-',(char*)&i+i.size);
		}	
		gloxUnimplemented();
		return nullptr;
	}

	void* pmmAllocZ()
	{
		auto addr = pmmAlloc();
		memset(addr, 0, arch::pageSize);
		return addr;
	}

	bool pmmFree(void* ptr)
	{
		// free is sooo much harder to do in this scenario
		// to optimize performance one should make a cache of address ranges that map to certain bitmap
		// without that we have to linear search everything which is slooow
		gloxUnimplemented();
	}
} // namespace glox

// static void memoryTest()
// {

//    void* arr[100];
//    for (auto& it : arr)
//    {
//       it = glox::pmmAlloc();

//    }

//    for (auto& it: arr)
//    {
//       if(glox::pmmFree(it) == false)
//          gloxLogln("pmmFree failed :( at index: ",(char*)it-(char*)arr);

//    }

// }

// registerTest(memoryTest);