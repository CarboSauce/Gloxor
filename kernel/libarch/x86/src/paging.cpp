#include "asm/paging.hpp"
#include "asm/asmstubs.hpp"
#include "glox/bitfields.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "memory/virtmem.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"


using namespace arch;
using namespace x86::virt;
using namespace virt;

// Untill kernel gets more fancy, this is the default options we use
static constexpr u64 stdMask = writable | present | writeThrough;

static lvl2table identlvl2{};
static lvl2table kcodelvl2{};
static lvl3table identlvl3{};
static lvl3table kcodelvl3{};
static lvl4table klvl4{};
extern u8 _kernelFileBegin[];
extern u8 _kernelFileEnd[];
// physical base, virtual base, and their difference respectively
extern u64 kernelPhysOffset;
extern u64 kernelVirtOffset;
extern u64 kernelMappingOffset;
extern u64 physicalFbStart;
extern u64 physicalFbEnd;

inline u64 getPhysAddress(u64 virt)
{
	gloxAssert(virt >= arch::kernelHalfBase);
	u64 addr = virt + kernelMappingOffset;
	return addr;
}
inline u64 getPhysAddress(u64* virt)
{
	return getPhysAddress((u64)virt);
}

inline void mapKernel()
{
	auto start = (u64)_kernelFileBegin;
	virtCtxT ctx = (u64)klvl4.entries;
	size_t size = (_kernelFileEnd-_kernelFileBegin)/0x1000+1;
	gloxDebugLogln("size: ",size);
	for (size_t i = 0; i < size;++i)
	{
		map(ctx,_kernelFileBegin+i*0x1000,(void*)(kernelPhysOffset+i*0x1000));
	}
	// auto index4 = lvl4tableIndex(start);
	// auto index3 = lvl3tableIndex(start);
	// auto index2 = lvl2tableIndex(start);
	// auto lvl4ptr = &klvl4;
	// auto& lvl3entry = lvl4ptr->entries[index4];
	// lvl3entry = maskEntry(getPhysAddress(kcodelvl3.entries), stdMask);
	// auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
	// auto& lvl2entry = lvl3ptr->entries[index3];
	// lvl2entry = maskEntry(getPhysAddress(kcodelvl2.entries), stdMask);
	// auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);

	// gloxDebugLogln("lvl3: ", (void*)lvl3entry);
	// gloxDebugLogln("lvl2: ", (void*)lvl2entry);

	// size_t endingIndex = (_kernelFileEnd - _kernelFileBegin) / 0x200000 + 1;
	// gloxDebugLogln("Mapping kernel code from: ", _kernelFileBegin, " to: ", _kernelFileEnd,
	// 			   "\nNumber of indexes used to map is: ", endingIndex);

	// gloxAssert(index2 + endingIndex < 512, "Kernel spans more than 1GB");

	// for (size_t i = 0; i < endingIndex; ++i)
	// {
	// 	u64 offset = kernelPhysOffset & (~0x1fffff);
	// 	u64 entry = maskEntry(offset + i * 0x200000, stdMask | granul );
	// 	gloxDebugLogln("\n\nindex2: ",index2," offset: ",(void*)offset," entry: ",(void*)entry);
	// 	lvl2ptr->entries[index2 + i] = entry;
	// }
}

static void identityMap()
{
	auto index4 = 0; // lvl4tableIndex(start);
	auto index3 = 0; // lvl3tableIndex(start);
	auto lvl4ptr = &klvl4;
	auto& lvl3entry = lvl4ptr->entries[index4];
	lvl3entry = maskEntry(getPhysAddress(identlvl3.entries), stdMask);
	auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
	auto& lvl2entry = lvl3ptr->entries[index3];
	lvl2entry = maskEntry(getPhysAddress(identlvl2.entries), stdMask);
	auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
	/*
		Map lower 1GB, stupid, broken, fix by actually checking memory ranges we have to map
	*/
	for (u64 i = 0; i < 512; ++i)
	{
		lvl2ptr->entries[i] = maskEntry(i * 0x200000, stdMask | granul);
	}

	mapKernel();
}

namespace x86
{

	virtCtxT initKernelVirtMem()
	{
		virtCtxT context = getPhysAddress(klvl4.entries);
		gloxDebugLogln("Remapping CR3 to ",(void*)context);
		identityMap();
		auto [fbBeg, fbEnd] = glox::term::getUsedMemoryRange();
		auto* physFbBeg = (char*)physicalFbStart;
		auto* physFbEnd = (char*)physicalFbEnd;
		gloxDebugLogln("Physical fb range: ", physFbBeg, "-", physFbEnd);
		for (u64 i = 0; i < (u64)(fbEnd - fbBeg); i += 0x1000)
		{
			map(context, (char*)fbBeg + i, physFbBeg + i);
		}

		gloxDebugLogln(translate(context, (void*)0x1419242));
		gloxDebugLogln(translate(context, _kernelFileBegin));
		gloxDebugLogln(translate(context, _kernelFileEnd));
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
		if (!glox::bitmask(lvl3entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl3entry = maskEntry(freshAdr, stdMask);
		}
		auto* lvl3ptr = (lvl3table*)getPhysical(lvl3entry);
		auto& lvl2entry = lvl3ptr->entries[index3];
		if (!glox::bitmask(lvl2entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl2entry = maskEntry(freshAdr, stdMask);
		}
		auto* lvl2ptr = (lvl2table*)getPhysical(lvl2entry);
		auto& lvl1entry = lvl2ptr->entries[index2];
		if (!glox::bitmask(lvl1entry, present))
		{
			auto freshAdr = (u64)glox::pmmAllocZ();
			if (!freshAdr)
				return false;
			lvl1entry = maskEntry(freshAdr, stdMask);
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
		gloxDebugLogln("Setting the cr3 to: ", (void*)context);
		asm volatile("mov %0, %%cr3" ::"r"(maskEntry(context, writeThrough)));
	}
	void* translate(const virtCtxT context, const void* from)
	{
		gloxDebugLogln("Translating address ", from);
		auto intFrom = (uintptr)from;
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