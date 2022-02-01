#include "asm/paging.hpp"
#include "asm/asmstubs.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "memory/virtmem.hpp"
#include "system/logging.hpp"
#include "glox/bitfields.hpp"
#include "system/danger.hpp"


using namespace arch;
using namespace x86::virt;
using namespace virt;

// Untill kernel gets more fancy, this is the default options we use
static constexpr u64 stdMask = writable | present | writeThrough;


static lvl1table klvl1{};
static lvl2table klvl2{};
static lvl3table klvl3{};
static lvl3table klvl3lower{};
static lvl4table klvl4{};
extern u8 _kernelFileBegin[];
extern u8 _kernelFileEnd[];
// physical base, virtual base, and their difference respectively
extern u64 kernelPhysOffset;
extern u64 kernelVirtOffset;
extern u64 kernelMappingOffset;

inline u64 getPhysAddress(u64 virt)
{
	u64 addr = kernelPhysOffset + (virt - kernelVirtOffset);
	return addr;
}
inline u64 getPhysAddress(u64* virt)
{
	return getPhysAddress((u64)virt);
}

static void identityMap()
{
	auto start = (uintptr_t)_kernelFileBegin;
	auto index4 = lvl4tableIndex(start);
	auto index3 = lvl3tableIndex(start);
	auto index2 = lvl2tableIndex(start);

	auto lvl4ptr = &klvl4;
	auto& lvl3entry = lvl4ptr->entries[index4];
	lvl3entry = maskEntry(getPhysAddress(klvl3.entries), stdMask);
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
	auto& lvl2entry = lvl3ptr->entries[index3];
	lvl2entry = maskEntry(getPhysAddress(klvl2.entries), stdMask);
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
	/* 
		Map lower 200MB, stupid, broken, fix by actually checking memory ranges we have to map
	*/
	for (u64 i = 0; i < 100; ++i)
	{
		lvl2ptr->entries[i] = maskEntry(i * 0x200000, stdMask | granul);
	}

	/*
		TODO:
			This code is utterly broken and not generic at all,
			it assumes there is direct mapping between kernel virtual address and physical address
			preferably this code should 2MB map the kernel code 
			from [higherhalf,higherhalf+size]
			to   [physicalbase,physicalbase+size]
			there is potential problem where bootloader loads the kernel at 4KB alligned address,
			so to workaround it we have to allign the address down to 2MB boundary
	*/

	auto& lvl3entrylower = lvl4ptr->entries[0];
	lvl3entrylower = maskEntry(getPhysAddress(klvl3lower.entries), stdMask);
	auto* lvl3ptrlower = (lvl3table*)getPhysical(lvl3entrylower);
	auto& lvl2entrylower = lvl3ptrlower->entries[0];
	lvl2entrylower = lvl2entry;

	gloxDebugLog("Level 4 ", (void*)lvl4ptr, '\n');
	gloxDebugLog("Level 3 ", (void*)lvl3ptr, '\n');
	gloxDebugLog("Level 2 ", (void*)lvl2ptr, '\n');
}


namespace x86
{

	virtCtxT initKernelVirtMem()
	{
		gloxDebugLogln("Remapping CR3");
		identityMap();
		virtCtxT context = getPhysAddress(klvl4.entries);
		gloxDebugLogln(translate(context, _kernelFileBegin));
		gloxDebugLogln(translate(context, _kernelFileEnd));
		gloxDebugLogln(translate(context, (void*)0x300000));
		setContext(context);
		return context;
	}
} // namespace x86

namespace virt
{

	/*
	   on x86, paging looks like a tree, so we need to traverse the tree of height 5
	 */
	bool map(virtCtxT context, const void* from, const void* to)
	{
		auto index4 = lvl4tableIndex((uintptr_t)from);
		auto index3 = lvl3tableIndex((uintptr_t)from);
		auto index2 = lvl2tableIndex((uintptr_t)from);
		auto index1 = lvl1tableIndex((uintptr_t)from);

		auto lvl4ptr = (lvl4table*)context;
		auto& lvl3entry = lvl4ptr->entries[index4];
		if (!glox::bitmask(lvl3entry,present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr) return false;
			lvl3entry = maskEntry(freshAdr,stdMask);
		}
		auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
		auto& lvl2entry = lvl3ptr->entries[index3];
		if (!glox::bitmask(lvl2entry,present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr) return false;
			lvl2entry = maskEntry(freshAdr,stdMask);
		}
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
		auto& lvl1entry = lvl2ptr->entries[index2];
		if (!glox::bitmask(lvl1entry,present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr) return false;
			lvl1entry = maskEntry(freshAdr,stdMask);
		}
		auto* lvl1ptr = (lvl1table*)getPhysical(lvl1entry);
		lvl1ptr->entries[index1] = maskEntry((u64)to, stdMask);
		return true;
	}
	bool unmap(virtCtxT context, const void* whichVirtual)
	{
		(void)context;
		(void)whichVirtual;
		return false;
	}
	void setContext(virtCtxT context)
	{
		gloxDebugLogln("Setting the cr3 to ", (void*)context);
		asm volatile("mov %0, %%cr3" ::"r"(maskEntry(context, writeThrough)));
	}
	void* translate(const virtCtxT context, const void* from)
	{
		gloxDebugLogln("Translating address ", from);
		auto index4 = lvl4tableIndex((uintptr_t)from);
		auto index3 = lvl3tableIndex((uintptr_t)from);
		auto index2 = lvl2tableIndex((uintptr_t)from);
		auto index1 = lvl1tableIndex((uintptr_t)from);
		auto* lvl4ptr = (lvl4table*)context;
		auto* lvl3ptr = (lvl3table*)getPhysical(lvl4ptr->entries[index4]);
		// getPhysical would discard page granuality so we have to be careful
		auto lvl3entry = lvl3ptr->entries[index3];
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl3entry);
		if ((lvl3entry & granul) != 0)
			return lvl2ptr;
		auto lvl2entry = lvl2ptr->entries[index2];
		auto* lvl1ptr = (lvl1table*)getPhysical(lvl2entry);
		if ((lvl2entry & granul) != 0)
			return lvl1ptr;
		return reinterpret_cast<void*>(getPhysical(lvl1ptr->entries[index1]));
	}
	bool virtInitContext(virtCtxT)
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