#include "virtmem.hpp"
#include "arch/addrspace.hpp"
#include "system/logging.hpp"
#include "gloxor/kinfo.hpp"
#include "glox/linkedlist.hpp"
#include "system/terminal.hpp"
using namespace arch::vmem;
using namespace glox;
using namespace arch;
namespace glox
{
VSpace kAddrSpace{};
inline auto to_mask(PagePrivileges p, PageCaching c)
{
	return (u64)p | (u64)c;
}
bool glox::VSpace::map(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable,from,to,to_mask(pv,cm));
}
bool glox::VSpace::map_huge(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable,from,to,to_mask(pv,cm));
}
void glox::VSpace::make_current()
{
	virt_set_context(ptable);
}	
paddrT glox::VSpace::translate(vaddrT from)
{
	return arch::vmem::translate(ptable,from);
}

}

inline void map_region(vaddrT from, vaddrT to, paddrT start,PagePrivileges pp,PageCaching pc)
{
	auto isalign = [](auto a, auto b)
	{ return a % b == 0; };

	while (from < to)
	{

		if (to - from > 0x200'000 && isalign(from, 0x200'000))
		{
			kAddrSpace.map_huge(from, start,pp,pc);
			from += 0x200'000;
			start += 0x200'000;
		}
		else
		{
			kAddrSpace.map(from, start,pp,pc);
			from += 0x1000;
			start += 0x1000;
		}
	}
}
inline void map_kernel()
{	
	size_t size = (kernelFileEnd - kernelFileBegin);
	gloxDebugLogln("kernelPhysOffset: ", kernelPhysOffset);
	for (size_t i = 0; i < size; i += pageSize)
	{
		kAddrSpace.map((vaddrT)kernelFileBegin + i, kernelPhysOffset + i, PagePrivileges::all, PageCaching::writeThrough);
	}
}
inline void identity_map()
{
	for (const auto& it : glox::machineInfo.mmapEntries)
	{
		if (it.type == BootInfo::MemTypes::usable || it.type == BootInfo::MemTypes::reclaimable)
		{
			const auto from = it.base + physicalMemBase;
			const auto to = it.base;
			map_region(from, from + it.length, to,PagePrivileges::all,PageCaching::writeThrough);
		}
	}
}

void init_addr_space()
{
	gloxDebugLogln("Remapping CR3 to ", (void*)&kAddrSpace);
	identity_map();
	auto [fbeg, fend] = glox::term::get_used_memory_range();
	gloxDebugLogln("Mapping framebuffer from: ", fbeg, " to: ", fend);
	map_region((vaddrT)fbeg, (paddrT)fend, get_real_data_addr((paddrT)fbeg),PagePrivileges::readWrite,PageCaching::writeCombine);
	map_kernel();
	gloxDebugLogln("Trying translation code, from : ", fbeg, " to: ", (void*)kAddrSpace.translate((u64)fbeg));
	gloxDebugLogln("Trying translation code, from : ", (u8*)physicalMemBase + 0x200'000, " to: ", (void*)kAddrSpace.translate(physicalMemBase + 0x200'000));
	//lets goo
}
