#pragma once
#include "arch/paging.hpp"
#include "gloxor/types.hpp"

namespace gx {
class alignas(arch::vmem::pageSize) VSpace
{
    // arch defined handle
    arch::vmem::vmemCtxT ptable;

public:
    using CacheMode = arch::vmem::PageCaching;
    using Privileges = arch::vmem::PagePrivileges;
    bool map(vaddr from, paddr to, Privileges, CacheMode);
    bool map_huge(vaddr from, paddr to, Privileges, CacheMode);
    bool unmap(vaddr whichVirtual);
    paddr translate(vaddr from);
    void make_current();
    VSpace() = default;
    static VSpace* create();
    void destroy();
    VSpace(const VSpace&) = delete;
    VSpace& operator=(const VSpace&) = delete;
};

extern VSpace kAddrSpace;
[[nodiscard]] void* vmap_iomem(paddr base, size_t len, arch::vmem::PageCaching);
void vunmap_iomem(void* ptr);
} // namespace gx
