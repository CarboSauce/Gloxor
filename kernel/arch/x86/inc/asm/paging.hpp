#pragma once
#include "arch/addrspace.hpp"
#include "gloxor/types.hpp"
#include <cstddef>

// TODO: this namespace name is so bad, change it
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

constexpr u64 getPhysical(u64 entry)
{
	return (entry & 0x000f'ffff'ffff'f000);
}

constexpr u64 maskEntry(u64 physAddr, u64 mask)
{
	return (getPhysical(physAddr)) | mask;
}
constexpr size_t pteIndex(u64 addr, pteShift shiftval)
{
	return (addr >> static_cast<int>(shiftval)) & 0x1ff;
}

inline u64* getNextPte(pagetable* table, u64 addr, pteShift shiftval)
{
	gloxAssert(table != nullptr);
	const auto tab = (pagetable*)arch::toVirt((u64)table);
	const auto index = pteIndex(addr, shiftval);
	return &(tab->entries[index]);
}

enum pageLevel
{
	lvl1 = 1,
	lvl2,
	lvl3,
	lvl4,
	lvl5
};

template <size_t I>
struct alignas(0x1000) pageTable;

template <size_t I>
struct pageEntry
{
	constexpr static size_t lvl = I;
	u64 entry;
	void set(paddrT addr, pagingBits mask)
	{
		entry = (addr & 0x000ffffffffff000) | mask;
	}
	auto paddr() const
	{
		return entry & 0x000ffffffffff000;
	}
	const auto* vaddr() const
	{
		auto realaddr = paddr();
		if (entry < arch::physicalMemBase)
			realaddr += arch::physicalMemBase;
		return reinterpret_cast<pageTable<pageLevel(lvl - 1)>*>(realaddr);
	}
	auto vaddr()
	{
		// :tf:
		return const_cast<pageTable<pageLevel(lvl - 1)>*>(static_cast<const pageEntry<I>&>(*this).vaddr());
	}
};

/**
 * @brief Type used for managing page tables
 * slowly shift towards using this type for all ptable manipulations
 */
template <size_t I>
struct alignas(0x1000) pageTable
{
	static constexpr size_t lvl = I;
	static_assert(lvl > 0 && lvl < 5, "Page table level must be between 1 and 4");
	static constexpr size_t shift = lvl * 9 + 3;
	pageEntry<I> entries[512];

	auto begin()
	{
		return entries;
	}
	auto end()
	{
		return entries + 512;
	}
	auto index(glox::vaddrT addr) const
	{
		return (addr >> shift) & 0x1ff;
	}
	const pageEntry<I>& entry(glox::vaddrT addr) const
	{
		return entries[index(addr)];
	}
	pageEntry<I>& entry(glox::vaddrT addr)
	{
		// :tf:
		return const_cast<pageEntry<I>&>(static_cast<const pageTable<I>&>(*this).entry(addr));
	}
	/**
	 * @brief Obtains lower level table from the provided addr
	 * @param addr Address used for retrieving next table
	 * @return Next table of lower level
	 */
	const auto* next(glox::vaddrT addr) const
	{
		static_assert(I > pageLevel::lvl1, "Can't get next table from lvl1 page table");
		return entries[index(addr)].vaddr();
	}
	auto next(glox::vaddrT addr)
	{
		// :tf:
		return const_cast<pageEntry<I>&>(static_cast<const pageTable<I>&>(*this).next(addr));
	}
};

} // namespace x86::vmem
