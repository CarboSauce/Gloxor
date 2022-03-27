#pragma once
#include "gloxor/types.hpp"
extern u8 _kernelFileBegin[];
extern u8 _kernelFileEnd[];   
namespace arch
{
	constexpr u64 kernelHalfBase = 0xffffffff80000000;
	constexpr u64 physicalMemBase = 0xffff800000000000;
	extern uintptr kernelPhysOffset;
   extern uintptr kernelVirtOffset;
   extern uintptr kernelMappingOffset;
} // namespace arch