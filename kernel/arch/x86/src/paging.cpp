#include "asm/paging.hpp"
#include "arch/addrspace.hpp"
#include "arch/paging.hpp"
#include "asm/asmstubs.hpp"
#include "asm/msr.hpp"
#include "glox/assert.hpp"
#include "gloxor/kinfo.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
/*
	on x86, paging looks like a tree, so we need to traverse the tree of height 5
*/

using namespace arch::vmem;
using namespace arch;
using namespace x86::vmem;
using namespace glox;

static PageTable<PageLevel::lvl4> klvl4{};

inline void map_region(vmemCtxT ctx, vaddrT from, vaddrT to, paddrT start)
{
	auto isalign = [](auto a, auto b)
	{ return a % b == 0; };

	while (from < to)
	{

		if (to - from > 0x200'000 && isalign(from, 0x200'000))
		{
			map_huge_page(ctx, from, start);
			from += 0x200'000;
			start += 0x200'000;
		}
		else
		{
			map(ctx, from, start);
			from += 0x1000;
			start += 0x1000;
		}
	}
}

inline void map_kernel()
{
	auto ctx = (vmemCtxT)klvl4.entries;
	size_t size = (kernelFileEnd - kernelFileBegin);
	gloxDebugLogln("kernelPhysOffset: ", kernelPhysOffset);
	for (size_t i = 0; i < size; i += pageSize)
	{
		map(ctx, (vaddrT)kernelFileBegin + i, kernelPhysOffset + i, defFlags);
	}
}

static void identity_map()
{
	for (const auto& it : glox::machineInfo.mmapEntries)
	{
		if (it.type == BootInfo::MemTypes::usable || it.type == BootInfo::MemTypes::reclaimable)
		{
			const auto from = it.base + physicalMemBase;
			const auto to = it.base;
			map_region((u64)klvl4.entries, from, from + it.length, to);
		}
	}
}
inline bool setup_pat()
{
	auto info = cpuid(1);
	if (!(info.edx & (1 << 16)))
		return false;
	// UC WC WT WB the CPU default
	gloxDebugLogln("Current ia32PAT is: ", (void*)rdmsr(msr::ia32PAT));
	u32 patlow = 0x00010406;
	// UC UC- WP WC
	u32 pathigh = 0x00070105;
	wrmsr(msr::ia32PAT, patlow, pathigh);
	return true;
}

template <size_t I>
glox::pair<const PageTable<I - 1>*, bool> translate_single_entry(const PageTable<I>& ctx, vaddrT from)
{
	auto entry = ctx.entry(from);
	if (!(entry.entry & present))
		return {nullptr, true};
	else if (entry.entry & granul)
		return {(const PageTable<I - 1>*)entry.paddr(), true};
	return {entry.vaddr(), false};
}

namespace x86
{
vmemCtxT init_kernel_virt_mem()
{
	auto context = (vmemCtxT)klvl4.entries;
	gloxDebugLogln("Remapping CR3 to ", (void*)klvl4.entries, " phys address: ", (void*)context);
	identity_map();
	auto [fbeg, fend] = glox::term::get_used_memory_range();
	gloxDebugLogln("Mapping framebuffer from: ", fbeg, " to: ", fend);
	map_region(context, (vaddrT)fbeg, (paddrT)fend, get_real_data_addr((paddrT)fbeg));
	map_kernel();
	if (setup_pat())
		gloxDebugLogln("PAT supported on boot cpu");
	virt_set_context(context);
	gloxDebugLogln("Trying translation code, from : ", fbeg, " to: ", (void*)translate(context, (u64)fbeg));
	gloxDebugLogln("Trying translation code, from : ", (u8*)physicalMemBase + 0x200'000, " to: ", (void*)translate(context, physicalMemBase + 0x200'000));

	return context;
}
} // namespace x86

namespace arch::vmem
{
/*
	  TODO: Completely unreadable, reconsider rewritting with member functions
*/

inline bool alloc_page_if_needed(u64& entry, u64 mask)
{
	if (entry & present)
		return true;
	const auto freshAdr = (u64)glox::page_alloc();
	if (!freshAdr)
		return false;
	entry = mask_entry(get_real_address(freshAdr), mask);
	return true;
}

bool map_huge_page(vmemCtxT context, vaddrT from, paddrT to, u64 mask)
{
	auto* ctx = (PageTable<4>*)context;
	auto& e4 = ctx->entry(from);
	gloxAssert(alloc_page_if_needed(e4.entry, mask));
	auto& e3 = e4.vaddr()->entry(from);
	gloxAssert(alloc_page_if_needed(e3.entry, mask));
	e3.vaddr()->entry(from).entry = mask_entry(to & ~0x100000, mask | granul);
	return true;
}

bool map(vmemCtxT context, vaddrT from, paddrT to, u64 mask)
{
	auto* ctx = (PageTable<4>*)context;
	auto& e4 = ctx->entry(from);
	gloxAssert(alloc_page_if_needed(e4.entry, mask));
	auto& e3 = e4.vaddr()->entry(from);
	gloxAssert(alloc_page_if_needed(e3.entry, mask));
	auto& e2 = e3.vaddr()->entry(from);
	gloxAssert(alloc_page_if_needed(e2.entry, mask));
	e2.vaddr()->entry(from).entry = mask_entry(get_physical(to), mask);
	return true;
}

bool unmap(vmemCtxT context, const void* whichVirtual)
{
	(void)context;
	(void)whichVirtual;
	return false;
}

paddrT translate(vmemCtxT pt, vaddrT from)
{
	auto* ctx = (const PageTable<4>*)pt;
	auto e4 = translate_single_entry(*ctx, from);
	if (e4.second)
		return (paddrT)e4.first;
	auto e3 = translate_single_entry(*e4.first, from);
	if (e3.second)
		return (paddrT)e3.first;
	auto e2 = translate_single_entry(*e3.first, from);
	if (e2.second)
		return (paddrT)e2.first;
	auto e1 = e2.first->entry(from);
	if (!(e1.entry & present))
		return 0;
	return e1.paddr();
}

vmemCtxT virt_create_context()
{
	return (u64)glox::page_alloc();
}

} // namespace arch::vmem
