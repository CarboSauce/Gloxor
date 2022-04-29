#pragma once
#include "gloxor/types.hpp"
#include "system/logging.hpp"
extern u8 kernelFileBegin[];
extern u8 kernelFileEnd[];
namespace arch
{
	constexpr u64 kernelMemBase = 0xffffffff80000000;
	constexpr u64 physicalMemBase = 0xffff800000000000;
	extern uintptr kernelPhysOffset;
	extern uintptr kernelVirtOffset;
	extern uintptr kernelMappingOffset;

	inline u64 getRealKernelAddr(u64 virt)
	{
		gloxAssert(virt >= arch::kernelMemBase);
		return virt + kernelMappingOffset;
	}
	inline u64 getRealKernelAddr(const void* virt)
	{
		return getRealKernelAddr((u64)virt);
	}
	inline u64 getRealDataAddr(u64 virt)
	{
		gloxAssert(virt >= arch::physicalMemBase);
		return virt - arch::physicalMemBase;
	}

	inline u64 getRealDataAddr(const void* virt)
	{
		return getRealDataAddr((u64)virt);
	}
	inline u64 getRealAddress(u64 virt)
	{
		if (virt >= arch::kernelMemBase  ) return getRealKernelAddr(virt);
		if (virt >= arch::physicalMemBase) return getRealDataAddr(virt);
		return virt;
	}


} // namespace arch