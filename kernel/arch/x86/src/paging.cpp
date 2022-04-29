#include "asm/paging.hpp"
#include "arch/addrspace.hpp"
#include "asm/asmstubs.hpp"
#include "asm/msr.hpp"
#include "glox/bitfields.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "memory/virtmem.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
/*
   on x86, paging looks like a tree, so we need to traverse the tree of height 5
*/

using namespace arch::vmem;
using namespace arch;
using namespace x86::vmem;
using namespace glox;

// Untill kernel gets more fancy, this is the default options we use

static lvl2table identlvl2{};
static lvl2table kcodelvl2{};
static lvl3table identlvl3{};
static lvl3table kcodelvl3{};
static lvl4table klvl4{};

inline void mapKernel()
{
	auto ctx = (vmemCtxT)klvl4.entries;
	size_t size = (kernelFileEnd - kernelFileBegin)/0x1000+1;
	gloxDebugLogln("size: ",size);
	for (size_t i = 0; i < size;++i)
	{
		map(ctx, kernelFileBegin +i*0x1000,(void*)(kernelPhysOffset+i*0x1000), defFlags);
	}
}

static void identityMap()
{
	auto index4 = 0; // lvl4tableIndex(start);
	auto index3 = 0; // lvl3tableIndex(start);
	auto lvl4ptr = &klvl4;
	auto& lvl3entry = lvl4ptr->entries[index4];
	lvl3entry = maskEntry(getRealKernelAddr(identlvl3.entries), defFlags);
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
	auto& lvl2entry = lvl3ptr->entries[index3];
	lvl2entry = maskEntry(getRealKernelAddr(identlvl2.entries), defFlags);
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
	/*
		Map lower 1GB, stupid, broken, fix by actually checking memory ranges we have to map
	*/
	for (u64 i = 0; i < 512; ++i)
	{
		lvl2ptr->entries[i] = maskEntry(i * 0x200000, defFlags | granul);
	}

	mapKernel();
}
inline bool setupPAT()
{
	auto info = cpuid(1);
	if (!(info.edx & (1 << 16))) return false;
	// UC UC- WT WB the CPU default
	gloxDebugLogln("Current ia32PAT is: ",(void*)rdmsr(msr::ia32PAT));
	u32 patlow  = 0x00070406;
	// UC UC- WP WC 
	u32 pathigh = 0x00070105;
	wrmsr(msr::ia32PAT,patlow,pathigh);
	return true;
}

namespace x86
{
	vmemCtxT initKernelVirtMem()
	{
		auto context = getRealKernelAddr(klvl4.entries);
		gloxDebugLogln("Remapping CR3 to ",(void*)context);
		identityMap();
		auto fbrange = glox::term::getUsedMemoryRange();
		
		gloxDebugLogln("Mapping framebuffer");
		for (const auto& i : fbrange)
		{
			map(context,&i,(void*)getRealDataAddr(&i));
		}

		gloxDebugLogln(translate(context, (void*)0x0));
		gloxDebugLogln(translate(context, kernelFileBegin));
		gloxDebugLogln(translate(context, kernelFileEnd));

		if (setupPAT()) gloxDebugLogln("PAT supported on boot cpu");
		setContext(context);
		return context;
	}
} // namespace x86

namespace arch::vmem
{
	/*
		Allocator seems to work, but because of PAT flag being the same
		as granuality flag, one cant actually allocate page with PAT set

		workaround is to completely rewrite memory manager
		perhaps x86 paging shouldnt leak to kernel code
	*/
	bool map(vmemCtxT context, const void* from, const void* to, u64 mask)
	{
		auto index4 = lvl4tableIndex((u64)from);
		auto index3 = lvl3tableIndex((u64)from);
		auto index2 = lvl2tableIndex((u64)from);
		auto index1 = lvl1tableIndex((u64)from);
		auto lvl4ptr = (lvl4table*)context;
		auto& lvl3entry = lvl4ptr->entries[index4];
		if (!glox::bitmask(lvl3entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl3entry = maskEntry(freshAdr, mask);
		}
		auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
		auto& lvl2entry = lvl3ptr->entries[index3];
		if (!glox::bitmask(lvl2entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl2entry = maskEntry(freshAdr, mask);
		}
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
		auto& lvl1entry = lvl2ptr->entries[index2];
		if (!glox::bitmask(lvl1entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl1entry = maskEntry(freshAdr, mask);
		}
		auto* lvl1ptr = (lvl1table*)getPhysical(lvl1entry);
		lvl1ptr->entries[index1] = maskEntry((u64)to, mask);
		return true;
	}

	bool unmap(vmemCtxT context, const void* whichVirtual)
	{
		(void)context;
		(void)whichVirtual;
		return false;
	}
	void setContext(vmemCtxT context)
	{
		gloxDebugLogln("Setting the cr3 to: ", (void*)maskEntry(context, writeThrough));
		asm volatile("mov %0, %%cr3" ::"r"(maskEntry(context, writeThrough)));
	}
	void* translate(const vmemCtxT context, const void* from)
	{
		gloxDebugLogln("Translating address ", from);
		const auto addr = (u64)from;
		const auto* lvl4ptr = (lvl4table*)getPhysical(context);
		auto* lvl3ptr = (lvl3table*)getPhysical(*getNextPte(lvl4ptr,addr,pteShift::lvl4));
		auto lvl3entry = *getNextPte(lvl3ptr,addr,pteShift::lvl3);
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
		if ((lvl3entry & granul) != 0)
		{
			const auto offset = addr & 0x3fffffff;
			return (char*)lvl2ptr + offset;
		}
		const auto lvl2entry = *getNextPte(lvl2ptr,addr,pteShift::lvl2);
		auto* lvl1ptr = (lvl1table*)getPhysical(lvl2entry);
		if ((lvl2entry & granul) != 0)
		{
			const auto offset = addr & 0x1fffff;
			return (char*)lvl1ptr + offset;
		}
		gloxDebugLogln("Its lvl1: ",lvl1ptr);
		const auto lvl1entry = *getNextPte(lvl1ptr,addr,pteShift::lvl1);
		const auto offset = addr & 0xfff;
		return reinterpret_cast<char*>(getPhysical(lvl1entry)) + offset;
	}
	bool virtInitContext(vmemCtxT)
	{
		return false;
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

} // namespace vmem