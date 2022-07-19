#include "asm/paging.hpp"
#include "arch/addrspace.hpp"
#include "arch/cpu.hpp"
#include "arch/paging.hpp"
#include "asm/asmstubs.hpp"
#include "asm/msr.hpp"
#include "glox/bitfields.hpp"
#include "gloxor/kinfo.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
/*
	on x86, paging looks like a tree, so we need to traverse the tree of height 5
*/

using namespace arch::vmem;
using namespace arch;
using namespace x86::vmem;
using namespace glox;

static lvl4table klvl4{};

inline void mapKernel()
{
	auto ctx = (vmemCtxT)klvl4.entries;
	size_t size = (kernelFileEnd - kernelFileBegin);
	gloxDebugLogln("kernelPhysOffset: ", kernelPhysOffset);
	for (size_t i = 0; i < size; i+=pageSize)
	{
		map(ctx, (vaddrT)kernelFileBegin + i, kernelPhysOffset + i, defFlags);
	}
}

static void identityMap()
{
	for (const auto& it : glox::machineInfo.mmapEntries)
	{
		if (it.type == bootInfo::memTypes::usable || 
			 it.type == bootInfo::memTypes::reclaimable)
		{
			for (sizeT i = 0; i < it.length; i += arch::vmem::pageSize)
			{
				const auto from = it.base + i + physicalMemBase;
				const auto to = it.base + i;
				map((u64)klvl4.entries, from,to);
			}
		}
	}
}
inline bool setupPAT()
{
	auto info = cpuid(1);
	if (!(info.edx & (1 << 16)))
		return false;
	// UC UC- WT WB the CPU default
	gloxDebugLogln("Current ia32PAT is: ", (void*)rdmsr(msr::ia32PAT));
	u32 patlow = 0x00070406;
	// UC UC- WP WC
	u32 pathigh = 0x00070105;
	wrmsr(msr::ia32PAT, patlow, pathigh);
	return true;
}

inline void mapRegion(vmemCtxT ctx, vaddrT from, vaddrT to, paddrT start)
{
	auto isalign = [](auto a, auto b){return a % b == 0;};

	while(from < to)
	{
		
		if (to - from > 0x200'000 && isalign(from,0x200'000)) 
		{
			gloxDebugLogln("Performing huge map");
			mapHugePage(ctx,from,start);
			from += 0x200'000;
			start += 0x200'000;
		}
		else 
		{
			map(ctx,from,start);
			from += 0x1000;
			start += 0x1000;
		}
	}
}

namespace x86
{
vmemCtxT initKernelVirtMem()
{
	auto context = getRealKernelAddr(klvl4.entries);
	gloxDebugLogln("Remapping CR3 to ", (void*)klvl4.entries, " phys address: ",(void*)context);
	identityMap();
	auto [fbeg,fend] = glox::term::getUsedMemoryRange();
	gloxDebugLogln("Mapping framebuffer from: ",fbeg," to: ",fbeg);
	mapRegion(context,(vaddrT)fbeg,(paddrT)fend,getRealDataAddr((paddrT)fbeg));
	// for (auto fbBeg = fbrange.begin(); fbBeg <= fbrange.end(); fbBeg+=pageSize)
	// {
	// 	map((u64)klvl4.entries, (vaddrT)fbBeg, getRealDataAddr(fbBeg));
	// }
	mapKernel();
	if (setupPAT())
		gloxDebugLogln("PAT supported on boot cpu");
	setContext(context);
	return context;
}
} // namespace x86

namespace arch::vmem
{
/*
	Completely unreadable, reconsider rewritting with member functions
*/
bool mapHugePage(vmemCtxT context, vaddrT from, paddrT to, u64 mask)
{
	auto* lvl4ptr = (lvl4table*)context;
	auto& lvl4entry = *getNextPte(lvl4ptr, from, pteShift::lvl4);
	if (!(lvl4entry & present))
	{
		const auto freshAdr = (u64)glox::pmmAllocZ();
		if (!freshAdr)
			return false;
		lvl4entry = maskEntry(getRealDataAddr(freshAdr), mask);
	}
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl4entry);
	auto& lvl3entry = *getNextPte(lvl3ptr, from, pteShift::lvl3);
	if (!(lvl3entry & present))
	{
		auto freshAdr = (u64)glox::pmmAllocZ();
		if (!freshAdr)
			return false;
		lvl3entry = maskEntry(getRealDataAddr(freshAdr), mask);
	}
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
	auto& lvl2entry = *getNextPte(lvl2ptr, from, pteShift::lvl2);
	lvl2entry = maskEntry(to & ~0x100000, mask | granul);
	return true;
}

bool map(vmemCtxT context, vaddrT from, paddrT to, u64 mask)
{
	auto* lvl4ptr = (lvl4table*)context;
	auto& lvl4entry = *getNextPte(lvl4ptr, from, pteShift::lvl4);
	if (!(lvl4entry & present))
	{
		const auto freshAdr = (u64)glox::pmmAllocZ();
		if (!freshAdr)
			return false;
		lvl4entry = maskEntry(getRealDataAddr(freshAdr), mask);
	}
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl4entry);
	auto& lvl3entry = *getNextPte(lvl3ptr, from, pteShift::lvl3);
	if (!(lvl3entry & present))
	{
		auto freshAdr = (u64)glox::pmmAllocZ();
		if (!freshAdr)
			return false;
		lvl3entry = maskEntry(getRealDataAddr(freshAdr), mask);
	}
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
	auto& lvl2entry = *getNextPte(lvl2ptr, from, pteShift::lvl2);
	if (!(lvl2entry & present))
	{
		auto freshAdr = (u64)glox::pmmAllocZ();
		if (!freshAdr)
			return false;
		lvl2entry = maskEntry(getRealDataAddr(freshAdr), mask);
	}
	auto* lvl1ptr = (lvl1table*)getPhysical(lvl2entry);
	*getNextPte(lvl1ptr, from, pteShift::lvl1) = maskEntry(getPhysical(to), mask );
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
void* translate(const vmemCtxT context, vaddrT from)
{
	const auto* lvl4ptr = (lvl4table*)getPhysical(context);
	auto* lvl3ptr = (lvl3table*)getPhysical(*getNextPte(lvl4ptr, from, pteShift::lvl4));
	auto lvl3entry = *getNextPte(lvl3ptr, from, pteShift::lvl3);
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
	if ((lvl3entry & granul) != 0)
	{
		const auto offset = from & 0x3fffffff;
		return (char*)lvl2ptr + offset;
	}
	const auto lvl2entry = *getNextPte(lvl2ptr, from, pteShift::lvl2);
	auto* lvl1ptr = (lvl1table*)getPhysical(lvl2entry);
	if ((lvl2entry & granul) != 0)
	{
		const auto offset = from & 0x1fffff;
		return (char*)lvl1ptr + offset;
	}
	gloxDebugLogln("Its lvl1: ", lvl1ptr);
	const auto lvl1entry = *getNextPte(lvl1ptr, from, pteShift::lvl1);
	const auto offset = from & 0xfff;
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

} // namespace arch::vmem
