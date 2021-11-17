#include "pmm.hpp"
#include <glox/bitfields.hpp>
#include <bit>
#include <glox/assert.hpp>
#include <gloxor/modules.hpp>
#include "system/logging.hpp"
namespace glox
{
   // This should point to context initialized by early protocol inits
   glox::pmmHeader* pmmCtx;
   glox::pmmHeader* initPmm(void* base, size_t length, glox::pmmHeader* ctx)
	{
		// TODO: Round the bitmap size to cover the most space,
		// and allign base on page size boundary
		// TODO: Merge consecutive to make memory continous
		size_t s = length;
		ctx = new ((void*)base) glox::pmmHeader;

		// We round size to multiple of 8 so that division by 8 doesnt cause truncation
		// Entries are page alligned so 4096 division is fine
		auto mul8size = ((s / 4096 + 7) & (~(size_t)7)) / 8;
		auto baseAligned = ((uintptr_t)base + sizeof(glox::pmmHeader) + mul8size);
		ctx->start = reinterpret_cast<void*>((baseAligned + 4095) & (~4095));
		std::uninitialized_fill_n(ctx->bitmap, mul8size, 0x0);
		auto sizePgDiv = s / 4096;
		auto sizePgMod = sizePgDiv % 8;
		// We mark parts of bitmap that point to not included range as allocated and procceed to never deallocate it
		// We can assume that sizePgDiv % 8 > 1 because otherwise bitmap wouldnt span to next page
		if (sizePgMod != 0)
		{
			for (size_t i = sizePgMod-1; i != 8; ++i)
			{
				ctx->bitmap[sizePgDiv / 8] |= 1 << (i);
			}
		}
		// We already used 1 page to allocate bookkeeping
		ctx->size = s - 4096;
		ctx->bitmapSize = mul8size;
		return ctx;
	};
 
   void* pmmAlloc()
   {
      for (auto&& it : *pmmCtx)
      {
         auto bound = it.bitmapSize;
         for (size_t i = 0; i != bound; ++i)
         {
            auto index = std::countr_one(it.bitmap[i]);
            if (index != 8)
            {
               it.bitmap[i] |= 1 << index;
               auto allocAdr = (uintptr_t)it.start+(i*8+index)*4096;
               gloxAssert(allocAdr < (uintptr_t)it.start+it.size,"Allocation out of bitmap bounds");
               gloxDebugLogln("Allocation Done, address: ",(void*)allocAdr," bitmap base: ",it.start);
               return reinterpret_cast<void*>(allocAdr);
            }
         }
      }
      return nullptr;
   }
   
   bool pmmFree(void* ptr)
   {
      // free is sooo much harder to do in this scenario
      // to optimize performance one should make a cache of address ranges that map to certain bitmap 
      // without that we have to linear search everything which is slooow
      
      for (auto& it : *pmmCtx)
      {
         //gloxDebugLogln("it.start: ",it.start);
         if (it.start > ptr) continue;
         const auto addr2free = ((uintptr_t)ptr-(uintptr_t)it.start)/4096;
         //gloxDebugLogln("Addr2free: ",addr2free,"Ptr to free: ",ptr," it.start: ",it.start);
         const auto i = addr2free / 8;
         const auto index = addr2free % 8;
         it.bitmap[i] &= ~(1<<index);
         return true;
      }
      return false;
   }
}

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