#pragma once
#include "gloxor/types.hpp"
#include "system/logging.hpp"
extern u8 kernelFileBegin[];
extern u8 kernelFileEnd[];
namespace arch
{
	constexpr u64 kernelMemBase = 0xffffffff80000000;
	constexpr u64 physicalMemBase = 0xffff800000000000;
	inline paddrT kernelPhysOffset;
	inline vaddrT kernelVirtOffset;
	inline sizeT kernelMappingOffset;

	inline paddrT getRealKernelAddr(vaddrT virt)
	{
		gloxAssert(virt >= arch::kernelMemBase);
		return virt + kernelMappingOffset;
	}
	inline paddrT getRealKernelAddr(const void* virt)
	{
		return getRealKernelAddr((u64)virt);
	}
	inline paddrT getRealDataAddr(vaddrT virt)
	{
		gloxAssert(virt >= arch::physicalMemBase);
		return virt - arch::physicalMemBase;
	}
	inline vaddrT toVirt(const paddrT addr)
	{
		if (addr < arch::physicalMemBase) return addr + arch::physicalMemBase;
		return addr;
	}

	inline paddrT getRealDataAddr(const void* virt)
	{
		return getRealDataAddr((u64)virt);
	}
	inline paddrT getRealAddress(vaddrT virt)
	{
		if (virt >= arch::kernelMemBase  ) return getRealKernelAddr(virt);
		if (virt >= arch::physicalMemBase) return getRealDataAddr(virt);
		return virt;
	}
} // namespace arch