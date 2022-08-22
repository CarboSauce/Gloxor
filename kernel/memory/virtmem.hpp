#pragma once
#include "arch/paging.hpp"
#include "gloxor/types.hpp"

namespace glox
{
class VSpace
{
	// arch defined handle 
	arch::vmem::vmemCtxT ptable;
	public:
	using CacheMode  = arch::vmem::PageCaching;
	using Privileges = arch::vmem::PagePrivileges;
	bool map(vaddrT from, paddrT to, Privileges, CacheMode, size_t count = 1)
	{
		/*
		 * define them as calling arch specific function that translates
		 * Attributes to paging structure
		*/
	}
	bool map_huge(vaddrT from, paddrT to, Privileges, CacheMode, size_t count = 1)
	{
		/*
		 * define them as calling arch specific function that translates
		 * Attributes to paging structure
		*/
	}
	bool unmap(vaddrT whichVirtual);
	paddrT translate(vaddrT from);
	VSpace(arch::vmem::vmemCtxT handle) : ptable(handle) {}
	static glox::optional<VSpace> create();
	void destroy();
	VSpace(const VSpace&) = delete;
	VSpace& operator=(const VSpace&) = delete;
};

extern VSpace kAddrSpace;
void* vmap_iomem(paddrT base, size_t len, arch::vmem::PageCaching);
} // namespace glox
