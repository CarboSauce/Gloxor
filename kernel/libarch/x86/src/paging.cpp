#include "asm/paging.hpp"
#include "arch/virtmem.hpp"
#include "gloxor/types.hpp"
#include "system/logging.hpp"


using namespace arch;
using namespace x86::virt;

struct arch::virtCtxImpl 
{
	lvl4table cr3;
};

// Untill kernel gets more fancy, this is the default options we use
static constexpr u64 stdMask = writable | present;
static u64 higherHalf = 0xffffffff80000000;

static lvl1table klvl1{};
static lvl2table klvl2{
   /* [0] =  */maskEntry((u64)klvl1.entries - higherHalf, stdMask)};
static lvl3table klvl3{
   /* [0] =  */maskEntry((u64)klvl2.entries - higherHalf, stdMask)};
static lvl4table klvl4{
   maskEntry((u64)klvl3.entries - higherHalf, stdMask)};
//static virtCtxImpl aha = {klvl4};
static virtContext ctx{(virtCtxImpl*)klvl4.entries};
extern u8 _kernelFileBegin[];
extern u8 _kernelFileEnd[];
namespace x86
{

	void initKernelVirtMem()
	{
		gloxLogln("Remapping CR3\n\n\n");
      u64 curcr3;
      asm volatile("mov %%cr3,%0":"=r"(curcr3));
      gloxLogln("Current CR3 value is = ",curcr3);
		auto* start = _kernelFileBegin;
		auto physicalStart = start - higherHalf;
		u64 end = _kernelFileEnd - _kernelFileBegin;
      gloxLogln("start = ",(u64)start);
      gloxLogln("end = ",(u64)end);
      gloxLogln("physicalStart = ",(u64)physicalStart);
      gloxDebugLogln("Level 4 ",(u64)klvl4.entries,' ');
      gloxDebugLogln("Level 3 ",(u64)klvl3.entries,' ');
      gloxDebugLogln("Level 2 ",(u64)klvl2.entries,' ');
      gloxDebugLogln("Level 1 ",(u64)klvl1.entries,' ');
      gloxDebugLogln("ctx ",(u64)&ctx,' ',ctx.context);
      gloxDebugLogln("(virtCtxImpl*)&klvl4 ", (virtCtxImpl*)&klvl4.entries);
		for (u64 i = 0; i < end + pageSize; i += pageSize)
		{
         auto loc = reinterpret_cast<const void*>(physicalStart + i);
         gloxLogln("Mapping ",(u64)(start+i)," to ",loc);
         ctx.map(start + i,loc);
		}
      for (int i = 0; i < 512; i++)
      {
         gloxLogln(i," entry of lvl1 is ",klvl1.entries[i]);
      }
      ctx.setContext();
	}
} // namespace x86

namespace arch
{

   /* 
      on x86, paging looks like a tree, so we need to traverse the tree of height 5
    */
	bool virtContext::map(const void* from, const void* to)
	{
		auto lvl4ptr = context->cr3;
		auto virtAdr = (uintptr_t)from;
		auto lvl3ptr = (lvl3table*)lvl4ptr.entries[lvl4ptr.index(virtAdr)];
      gloxDebugLogln("Level 4 ",(u64)lvl4ptr.entries,' ',lvl4ptr.index(virtAdr));
		auto lvl2ptr = (lvl2table*)lvl3ptr->entries[lvl3ptr->index(virtAdr)];
      gloxDebugLogln("Level 3 ",(u64)lvl3ptr->entries,' ',lvl3ptr->index(virtAdr));
		auto lvl1ptr = (lvl1table*)lvl2ptr->entries[lvl2ptr->index(virtAdr)];
      gloxDebugLogln("Level 2 ",(u64)lvl2ptr->entries,' ',lvl2ptr->index(virtAdr));
      gloxDebugLogln("Level 1 ",(u64)lvl1ptr->entries,' ',lvl1ptr->index(virtAdr));
		lvl1ptr->entries[lvl1ptr->index(virtAdr)] = maskEntry((u64)to, stdMask);
      return true;
	}
	bool virtContext::unmap(const void* whichVirtual)
	{
	}
	void virtContext::setContext()
	{
      gloxDebugLogln("Setting the cr3 to ",context);
		asm volatile("mov %0, %%cr3" ::"r"(context));
	}
	void* virtContext::translate(const void* from)
	{
	}
	bool virtInitContext(virtContext* init)
   {

   }

	void virtFlush(void* addr)
	{
		asm volatile("invlpg (%0)" ::"r"(addr));
	}

	void virtCacheFlush()
	{
		asm volatile("movq %%cr3, %%rax;mov %%rax,%%cr3" ::
						 : "rax");
	}

} // namespace arch