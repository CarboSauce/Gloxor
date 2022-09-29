#include "virtmem.hpp"
#include "memory/alloc.hpp"
#include "gloxor/kinfo.hpp"
#include "glox/linkedlist.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "arch/addrspace.hpp"
#include "arch/paging.hpp"
#include "glox/utilalgs.hpp"
#include "gloxor/test.hpp"
using namespace arch::vmem;
using namespace gx;
using namespace arch;

struct VmapRegion
{
	paddrT phys_base;
	vaddrT virt_base;
	size_t len;
	size_t flags;
	glox::list_node list_node;
};

glox::intrusive_list<VmapRegion> vmapList;
using VmapIter = glox::intrusive_list<VmapRegion>::iterator;
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
//inline VmapIter find_room(size_t len)
//{
//	for (auto& it : vmapList)
//	{
//		// TODO: Api wise libglox cant easilly get next entry
//		// gotta fix
//		if (it.virt_base+it.len+len < it.list_node.next->virt_base)
//		{
//			return &it;
//		}
//	}
//	return nullptr;
//}
namespace gx
{
void* vmap_iomem(paddrT base, size_t len, arch::vmem::PageCaching pc)
{
	auto nvmap = glox::find_if(vmapList.begin(),vmapList.end(),
			[len](auto&& it){return it.virt_base+it.len+len < VmapIter(&it).next()->virt_base;});
	auto newptr = gx::alloc<VmapRegion>();
	if (newptr == nullptr) return nullptr;
	newptr->virt_base = 0xdeadbeef;
	newptr->phys_base = base;
	newptr->len = len;
	newptr->flags = (size_t)pc; 
	// if (nvmap == nullptr)
	// {
	// 	vmapList.push_back(newptr);
	// }
	// else
	// {
	// 	vmapList.insert(VmapIter(nvmap), newptr);
	// }
	vmapList.insert(nvmap,newptr);

	return nullptr;
}
void vunmap_iomem(void* addr)
{
	auto res = glox::find_if(begin(vmapList),end(vmapList),
			[addr](auto&& it){return it.virt_base == (vaddrT)addr;});
	if (res == end(vmapList))
		return;
	vmapList.erase(res);
	gx::dealloc<VmapRegion>(res,1);
}
VSpace kAddrSpace{};
inline auto to_mask(PagePrivileges p, PageCaching c)
{
	return (u64)p | (u64)c;
}
bool gx::VSpace::map(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable, from, to, VMEM_MASK(pv, cm));
}
bool gx::VSpace::map_huge(vaddrT from, paddrT to, Privileges pv, CacheMode cm)
{
	return arch::vmem::map(kAddrSpace.ptable, from, to, VMEM_MASK(pv, cm));
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

#ifdef TEST
static bool ioremap_test()
{
	auto print_iomem = []()
	{
		for (const auto& it : vmapList)
		{
			gloxDebugLogln("Base: ",(void*)it.phys_base," Len: ", it.len," Next: ",it.list_node.next);
		}
	};
	gloxDebugLogln("Test 1\n");
	auto ptr1 = vmap_iomem(0xb8000,100,PageCaching::writeCombine);
	print_iomem();
	auto ptr2 = vmap_iomem(0xa8000,1000,PageCaching::writeCombine);
	gloxDebugLogln("Test 2\n");
	print_iomem();
	vunmap_iomem(ptr1);
	vunmap_iomem(ptr2);
	gloxDebugLogln("Test 3\n");
	print_iomem();
	return true;
}
registerTest("ioremap test",ioremap_test);
#endif

