#include "virtmem.hpp"
#include "memory/alloc.hpp"
#include "gloxor/kinfo.hpp"
#include "glox/linkedlist.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "arch/addrspace.hpp"
using namespace arch::vmem;
using namespace gx;
using namespace arch;

struct VmapRegion
{
	paddrT phys_base;
	vaddrT virt_base;
	size_t len;
	size_t flags;
	glox::list_node<VmapRegion> list_node;
};

glox::intrusive_list<VmapRegion> vmapList;
inline bool is_overlapping(uintptr base, uintptr back, uintptr obase, uintptr oback)
{
	return base <= oback && obase <= back;
}
inline bool is_there_dup(uintptr base,size_t len)
{
	for (const auto& it : vmapList)
	{
		if (is_overlapping(it.phys_base,it.phys_base+it.len,base,base+len))
		{
			return true;
		}
	}
	return false;
}
inline VmapRegion* find_room(size_t len)
{
	for (auto& it : vmapList)
	{
		// TODO: Api wise libglox cant easilly get next entry
		// gotta fix
		if (it.virt_base+it.len+len < it.list_node.next->virt_base)
		{
			return &it;
		}
	}
	return nullptr;
}
namespace gx
{
VSpace kAddrSpace{};
void* vmap_iomem(paddrT base, size_t len, arch::vmem::PageCaching pc)
{
	auto nvmap = find_room(len);
	auto newptr = gx::alloc<VmapRegion>();
	if (newptr == nullptr) return nullptr;
	newptr->virt_base = nvmap->virt_base+nvmap->len;
	newptr->phys_base = base;
	newptr->len = len;
	newptr->flags = (size_t)pc; 
	if (nvmap == nullptr)
	{
		vmapList.push_back(newptr);
	}
	else
	{
		vmapList.insert(glox::intrusive_list<VmapRegion>::iterator(nvmap), newptr);
	}

	(void)base;
	(void)len;
	(void)pc;
	return nullptr;
}
inline auto to_mask(PagePrivileges p, PageCaching c)
{
	return (u64)p | (u64)c;
}
bool gx::VSpace::map(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable, from, to, to_mask(pv, cm));
}
bool gx::VSpace::map_huge(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable, from, to, to_mask(pv, cm));
}
void gx::VSpace::make_current()
{
	arch::vmem::set_context(ptable);
}
paddrT gx::VSpace::translate(vaddrT from)
{
	return arch::vmem::translate(ptable, from);
}
} // namespace gx

