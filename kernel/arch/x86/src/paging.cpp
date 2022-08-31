#include "asm/paging.hpp"
#include "arch/addrspace.hpp"
#include "arch/paging.hpp"
#include "asm/asmstubs.hpp"
#include "glox/assert.hpp"
#include "gloxor/types.hpp"
#include "memory/pmm.hpp"
#include "memory/virtmem.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
/*
	on x86, paging looks like a tree, so we need to traverse the tree of height 5
*/
using namespace arch::vmem;
using namespace arch;
using namespace x86::vmem;
using namespace glox;

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

namespace arch::vmem
{
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

} // namespace arch::vmem
