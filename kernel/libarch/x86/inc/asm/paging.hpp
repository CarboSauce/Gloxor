#pragma once
#include "gloxor/types.hpp"

namespace x86::virt
{
	enum pagingBits
	{
		present = 1,
		writable = 1 << 1,
		user = 1 << 2,
		writeThrough = 1 << 3,
		cacheDisable = 1 << 4,
		accessed = 1 << 5,
		dirty = 1 << 6,
		granul = 1 << 7,
		globalPage = 1 << 8,
		ptePAT = 1 << 7,
		pdePAT = 1 << 12,
		pdptePAT = 1 << 12,
		execDisable = size_t(1) << 63
	};
	// gcc doesnt like it being part of enum
	struct alignas(0x1000) pml5t
	{
		u64 entries[512];
		size_t index(uintptr_t addr)
		{
			return (addr >> 48) & 0x1ff;
		}
	};
	struct alignas(0x1000) pml4t
	{
		u64 entries[512];
		size_t index(uintptr_t addr)
		{
			return (addr >> 39) & 0x1ff;
		}
	};
	struct alignas(0x1000) pdpt
	{
		u64 entries[512];
		size_t index(uintptr_t addr)
		{
			return (addr >> 30) & 0x1ff;
		}
	};
	struct alignas(0x1000) pdt
	{
		u64 entries[512];
		size_t index(uintptr_t addr)
		{
			return (addr >> 21) & 0x1ff;
		}
	};
	struct alignas(0x1000) pt
	{
		u64 entries[512];
		size_t index(uintptr_t addr)
		{
			return (addr >> 12) & 0x1ff;
		}
	};
	using lvl5table = pml5t;
	using lvl4table = pml4t;
	using lvl3table = pdpt;
	using lvl2table = pdt;
	using lvl1table = pt;

	constexpr u64 maskEntry(u64 physAddr, u64 mask)
	{
		return physAddr & mask;
	}
	constexpr u64 getPhysical(u64 entry)
	{
		return entry & (~0x1ff);
	}

} // namespace x86::virt