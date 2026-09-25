#pragma once
#include "gloxor/types.hpp"
#include "system/logging.hpp"
extern u8 kernelFileBegin[];
extern u8 kernelFileEnd[];
namespace arch {
constexpr u64 kernelMemBase = 0xffffffff80000000;
constexpr u64 physicalMemBase = 0xffff800000000000;
constexpr u64 virtMemBase = 0xffffe00000000000; // 96TB from the physbase
inline vaddr kernelPhysOffset;
inline vaddr kernelVirtOffset;
inline usize kernelMappingOffset;

inline vaddr get_real_kernel_addr(vaddr virt)
{
    gloxAssert(virt >= arch::kernelMemBase);
    return virt + kernelMappingOffset;
}
inline vaddr get_real_kernel_addr(const void* virt)
{
    return get_real_kernel_addr((u64)virt);
}
inline vaddr get_real_data_addr(vaddr virt)
{
    gloxAssert(virt >= arch::physicalMemBase);
    return virt - arch::physicalMemBase;
}
inline vaddr to_virt(vaddr addr)
{
    if (addr < arch::physicalMemBase)
        return addr + arch::physicalMemBase;
    return addr;
}

inline vaddr get_real_data_addr(const void* virt)
{
    return get_real_data_addr((u64)virt);
}
inline vaddr get_real_address(vaddr virt)
{
    if (virt >= arch::kernelMemBase)
        return get_real_kernel_addr(virt);
    if (virt >= arch::physicalMemBase)
        return get_real_data_addr(virt);
    return virt;
}
} // namespace arch
