#pragma once
#include "gloxor/types.hpp"
#include "system/logging.hpp"
extern u8 kernelFileBegin[];
extern u8 kernelFileEnd[];
namespace arch
{
constexpr u64 kernelMemBase = 0xffffffff80000000;
constexpr u64 physicalMemBase = 0xffff800000000000;
constexpr u64 virtMemBase = 0xffffe00000000000; // 96TB from the physbase
inline paddrT kernelPhysOffset;
inline vaddrT kernelVirtOffset;
inline sizeT kernelMappingOffset;

inline paddrT get_real_kernel_addr(vaddrT virt)
{
	gloxAssert(virt >= arch::kernelMemBase);
	return virt + kernelMappingOffset;
}
inline paddrT get_real_kernel_addr(const void* virt)
{
	return get_real_kernel_addr((u64)virt);
}
inline paddrT get_real_data_addr(vaddrT virt)
{
	gloxAssert(virt >= arch::physicalMemBase);
	return virt - arch::physicalMemBase;
}
inline vaddrT to_virt(paddrT addr)
{
	if (addr < arch::physicalMemBase)
		return addr + arch::physicalMemBase;
	return addr;
}

inline paddrT get_real_data_addr(const void* virt)
{
	return get_real_data_addr((u64)virt);
}
inline paddrT get_real_address(vaddrT virt)
{
	if (virt >= arch::kernelMemBase)
		return get_real_kernel_addr(virt);
	if (virt >= arch::physicalMemBase)
		return get_real_data_addr(virt);
	return virt;
}
} // namespace arch
