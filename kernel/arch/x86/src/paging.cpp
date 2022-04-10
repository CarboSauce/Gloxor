#include "asm/paging.hpp"
#include "asm/asmstubs.hpp"
#include "asm/msr.hpp"
#include "glox/bitfields.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "memory/virtmem.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "arch/segments.hpp"
/*
   on x86, paging looks like a tree, so we need to traverse the tree of height 5
*/

using namespace arch;
using namespace x86::virt;
using namespace virt;
using namespace glox;

// Untill kernel gets more fancy, this is the default options we use
static constexpr u64 defMask = writable | present ;

static lvl2table identlvl2{};
static lvl2table kcodelvl2{};
static lvl3table identlvl3{};
static lvl3table kcodelvl3{};
static lvl4table klvl4{};

inline u64 getPhysAddress(u64 virt)
{
	gloxAssert(virt >= arch::kernelHalfBase);
	return virt + kernelMappingOffset;
}
inline u64 getPhysAddress(const void* virt)
{
	return getPhysAddress((u64)virt);
}

inline u64 getPhysUserAddr(const u8* virt)
{
	auto addr = (u64)virt;
	gloxAssert(addr >= arch::physicalMemBase);
	return addr - arch::physicalMemBase;
}

inline void mapKernel()
{
	virtCtxT ctx = (u64)klvl4.entries;
	size_t size = (_kernelFileEnd-_kernelFileBegin)/0x1000+1;
	gloxDebugLogln("size: ",size);
	for (size_t i = 0; i < size;++i)
	{
		map(ctx,_kernelFileBegin+i*0x1000,(void*)(kernelPhysOffset+i*0x1000), defMask);
	}
}

static void identityMap()
{
	auto index4 = 0; // lvl4tableIndex(start);
	auto index3 = 0; // lvl3tableIndex(start);
	auto lvl4ptr = &klvl4;
	auto& lvl3entry = lvl4ptr->entries[index4];
	lvl3entry = maskEntry(getPhysAddress(identlvl3.entries), defMask);
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
	auto& lvl2entry = lvl3ptr->entries[index3];
	lvl2entry = maskEntry(getPhysAddress(identlvl2.entries), defMask);
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
	/*
		Map lower 1GB, stupid, broken, fix by actually checking memory ranges we have to map
	*/
	for (u64 i = 0; i < 512; ++i)
	{
		lvl2ptr->entries[i] = maskEntry(i * 0x200000, defMask | granul);
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
	virtCtxT initKernelVirtMem()
	{
		virtCtxT context = getPhysAddress(klvl4.entries);
		gloxDebugLogln("Remapping CR3 to ",(void*)context);
		identityMap();
		auto fbrange = glox::term::getUsedMemoryRange();
		
		gloxDebugLogln("Mapping framebuffer");
		for (const auto& i : fbrange)
		{
			map(context,&i,(void*)getPhysUserAddr(&i));
		}

		if (setupPAT()) gloxDebugLogln("PAT supported on boot cpu");
		setContext(context);
		return context;
	}
} // namespace x86

namespace virt
{

	/* 
		Allocator seems to work, but because of PAT flag being the same
		as granuality flag, one cant actually allocate page with PAT set
		
		workaround is to completely rewrite memory manager 
		perhaps x86 paging shouldnt leak to kernel code
	*/

	bool map(virtCtxT context, const void* from, const void* to, u64 mask)
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
	bool map(virtCtxT context, const void* from, const void* to)
	{
		return map(context,from,to,defMask);
	}

	bool unmap(virtCtxT context, const void* whichVirtual)
	{
		(void)context;
		(void)whichVirtual;
		return false;
	}
	void setContext(virtCtxT context)
	{
		gloxDebugLogln("Setting the cr3 to: ", (void*)maskEntry(context, writeThrough));
		asm volatile("mov %0, %%cr3" ::"r"(maskEntry(context, writeThrough)));
	}
	void* translate(const virtCtxT context, const void* from)
	{
		gloxDebugLogln("Translating address ", from);
		auto intFrom = (u64)from;
		auto index4 = lvl4tableIndex(intFrom);
		auto index3 = lvl3tableIndex(intFrom);
		auto index2 = lvl2tableIndex(intFrom);
		auto index1 = lvl1tableIndex(intFrom);
		auto* lvl4ptr = (lvl4table*)context;
		auto* lvl3ptr = (lvl3table*)getPhysical(lvl4ptr->entries[index4]);
		// getPhysical would discard page granuality so we have to be careful
		auto lvl3entry = lvl3ptr->entries[index3];
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
		if ((lvl3entry & granul) != 0)
		{
			auto offset = intFrom & 0x3fffffff;
			return (char*)lvl2ptr + offset;
		}
		auto lvl2entry = lvl2ptr->entries[index2];
		auto* lvl1ptr = (lvl1table*)getPhysical(lvl2entry);
		if ((lvl2entry & granul) != 0)
		{
			auto offset = intFrom & 0x1fffff;
			return (char*)lvl1ptr + offset;
		}
		auto offset = intFrom & 0xfff;
		return reinterpret_cast<char*>(getPhysical(lvl1ptr->entries[index1])) + offset;
	}
	bool virtInitContext(virtCtxT)
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

} // namespace virt