#pragma once
#include "glox/util.hpp"
#include "gloxor/types.hpp"

namespace x86::vmem
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
enum class pteShift
{
	lvl1 = 12, // 4KB page
	lvl2 = 21, // 2MB page
	lvl3 = 30, // 1GB page
	lvl4 = 39,
	lvl5 = 48
};

struct alignas(0x1000) pagetable
{
	u64 entries[512];
};
using pml5t = pagetable;
using pml4t = pagetable;
using pdpt = pagetable;
using pdt = pagetable;
using pt = pagetable;
using lvl5table = pml5t;
using lvl4table = pml4t;
using lvl3table = pdpt;
using lvl2table = pdt;
using lvl1table = pt;

constexpr size_t lvl5tableIndex(u64 addr)
{
	return (addr >> 48) & 0x1ff;
}
constexpr size_t lvl4tableIndex(u64 addr)
{
	return (addr >> 39) & 0x1ff;
}
constexpr size_t lvl3tableIndex(u64 addr)
{
	return (addr >> 30) & 0x1ff;
}
constexpr size_t lvl2tableIndex(u64 addr)
{
	return (addr >> 21) & 0x1ff;
}
constexpr size_t lvl1tableIndex(u64 addr)
{
	return (addr >> 12) & 0x1ff;
}
constexpr u64 getPhysical(u64 entry)
{
	return entry & (~0xfff);
}

constexpr u64 maskEntry(u64 physAddr, u64 mask)
{
	return (getPhysical(physAddr) /* physAddr & 0xffffff800 */) | mask;
}
constexpr size_t pteIndex(u64 addr, pteShift shiftval)
{
	return (addr >> static_cast<int>(shiftval)) & 0x1ff;
}

inline u64* getNextPte(pagetable* table, u64 addr, pteShift shiftval)
{
	gloxAssert(table != nullptr);
	const auto index = pteIndex(addr, shiftval);
	return &table->entries[index];
}
inline const u64* getNextPte(const pagetable* table, u64 addr, pteShift shiftval)
{
	gloxAssert(table != nullptr);
	const auto index = pteIndex(addr, shiftval);
	return &table->entries[index];
}

} // namespace x86::vmem