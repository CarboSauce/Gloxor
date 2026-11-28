export module arch.addrspace;
import gloxor.system;
import gloxor.types;
import std;

#include "glox/assert.hpp"

extern "C++"
{
export extern u8 kernelFileBegin[];
export extern u8 kernelFileEnd[];
}
export namespace arch {
// base of kernel mapped memory
inline constexpr u64 KERNEL_MEM_BASE = 0xffffffff80000000;
// base of identity mapped pages
inline constexpr u64 PHYSICAL_MEM_BASE = 0xffff800000000000;
inline constexpr u64 PHYSICAL_MEM_END = 0xffffe00000000000;
// 96TB from the physbase
inline constexpr u64 VIRT_MEM_BASE = PHYSICAL_MEM_END;
inline constexpr u64 VIRT_MEM_END
    = VIRT_MEM_BASE + 32uz * 1024 * 1024 * 1024 * 1024;
inline constexpr u64 PFN_DB_BASE = VIRT_MEM_END;
inline constexpr u64 PFN_DB_END = PFN_DB_BASE + 1024uz * 1024 * 1024 * 1024;

static_assert(KERNEL_MEM_BASE > PFN_DB_END);
inline vaddr kernelPhysOffset;
inline vaddr kernelVirtOffset;
inline usize kernelMappingOffset;

inline vaddr get_real_kernel_addr(vaddr virt)
{
    GLOX_ASSERT(virt >= arch::KERNEL_MEM_BASE);
    return virt + kernelMappingOffset;
}
inline vaddr get_real_kernel_addr(void const* virt)
{
    return get_real_kernel_addr((u64)virt);
}
inline vaddr get_real_data_addr(vaddr virt)
{
    GLOX_ASSERT(virt >= arch::PHYSICAL_MEM_BASE);
    return virt - arch::PHYSICAL_MEM_BASE;
}
inline vaddr to_virt(paddr addr)
{
    if (addr < arch::PHYSICAL_MEM_BASE)
        return addr + arch::PHYSICAL_MEM_BASE;
    return addr;
}

inline vaddr get_real_data_addr(void const* virt)
{
    return get_real_data_addr((u64)virt);
}
inline vaddr get_real_address(vaddr virt)
{
    if (virt >= arch::KERNEL_MEM_BASE)
        return get_real_kernel_addr(virt);
    if (virt >= arch::PHYSICAL_MEM_BASE)
        return get_real_data_addr(virt);
    return virt;
}
} // namespace arch
