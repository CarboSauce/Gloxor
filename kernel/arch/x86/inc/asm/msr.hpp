#pragma once
#include <glox/types.hpp>

namespace msr
{
	enum msr : uint32_t
	{
		ia32PAT = 0x00000277,  // Page Attribute Table
		ia32EFER = 0xC0000080, // EFER register, the non model specific one
		fsBase = 0xC0000100,   // FS Base register
		gsBase = 0xC0000101,   // GS Base register, used for cpu local vars
		kgsBase = 0xC0000102,  // Kernel GS Base
	};
};

inline uint64_t rdmsr(uint32_t msr)
{
	uint32_t low, high;
	asm volatile("rdmsr"
				 : "=a"(low), "=d"(high)
				 : "c"(msr));
	return ((uint64_t)high << 32) | low;
}
inline void wrmsr(uint32_t msr, uint64_t value)
{
	uint32_t low = value & 0xFFFFFFFF;
	uint32_t high = value >> 32;
	asm volatile("wrmsr" ::"c"(msr), "a"(low), "d"(high));
}
inline void wrmsr(uint32_t msr, uint32_t low, uint32_t high)
{
	asm volatile("wrmsr" ::"c"(msr), "a"(low), "d"(high));
}