#pragma once
#include <asm/paging.hpp>
#include <gloxor/types.hpp>
namespace arch::vmem
{
using pagingT = u64;
using vpageFlags = u64;

constexpr u64 pageSize = 0x1000;
static constexpr vpageFlags defFlags = x86::vmem::writable | x86::vmem::present;
/**
 * @brief class for managing virtual memory context
 *
 */
using vmemCtxT = arch::vmem::pagingT;
/**
 * @brief Map virtual address to physical address
 *
 * @param from Virtual address to map from
 * @param to Physical address to map to
 * @return true Success
 * @return false Mapping failed
 */

bool mapHugePage(vmemCtxT, vaddrT from, paddrT to, vpageFlags flags = arch::vmem::defFlags);
bool map(vmemCtxT, vaddrT from, paddrT to, vpageFlags flags = arch::vmem::defFlags);
/**
 * @brief Unmap virtual address from current context
 *
 * @param whichVirtual which virtual address to unamp
 * @return true Success
 * @return false Unmapping failed, if even possible
 */
bool unmap(vmemCtxT, vaddrT whichVirtual);
/**
 * @brief Translate virtual address to physical address
 *
 * @param from Virtual address to translate
 * @return void* Physical Address from translation
 */
paddrT translate(vmemCtxT, vaddrT from);
/**
 * @brief Initialize Virtual memory context
 * @note Possibly allocating, hence we need to return error
 * @note I'd like it to be a constructor, but we dont have exception handling to propagate error
 * @return true Success
 * @return false Allocation failure
 */
vmemCtxT virtCreateContext();

vmemCtxT virtDestroyContext(vmemCtxT context);

inline vmemCtxT virtGetCotext()
{
	vmemCtxT ctx;
	asm volatile("mov %%cr3,%0"
					 : "=r"(ctx));
	return ctx;
}
inline void virtSetContext(vmemCtxT context)
{
	asm volatile("mov %0, %%cr3" ::"r"(maskEntry(getRealAddress(context), x86::vmem::writeThrough)));
}
inline void virtFlush(void* addr)
{
	asm volatile("invlpg (%0)" ::"r"(addr)
					 : "memory");
}
inline void virtCacheFlush()
{
	asm volatile("movq %%cr3, %%rax;mov %%rax,%%cr3" ::
						  : "rax", "memory");
}
} // namespace arch::vmem