#pragma once
#include <asm/paging.hpp>
#include <gloxor/types.hpp>
#include <glox/util.hpp> 
namespace arch::vmem
{
using pagingT = u64;
using vpageFlags = u64;

constexpr u64 pageSize = 0x1000;
static constexpr vpageFlags defFlags = x86::vmem::writable | x86::vmem::present;
// enum class CacheMode
// {
// 	cacheDisabled,
// 	writeThrough,
// 	writeCombine
// };
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

bool map_huge_page(vmemCtxT, vaddrT from, paddrT to, vpageFlags flags = arch::vmem::defFlags);
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
vmemCtxT virt_create_context();

vmemCtxT virt_destroy_context(vmemCtxT context);

inline vmemCtxT virt_get_cotext()
{
	vmemCtxT ctx;
	asm volatile("mov %%cr3,%0"
					 : "=r"(ctx));
	return ctx;
}
inline void virt_set_context(vmemCtxT context)
{
	asm volatile("mov %0, %%cr3" ::"r"(mask_entry(get_real_address(context), x86::vmem::writeThrough)));
}
inline void virt_flush(void* addr)
{
	asm volatile("invlpg (%0)" ::"r"(addr)
					 : "memory");
}
inline void virt_cache_flush()
{
	asm volatile("movq %%cr3, %%rax;mov %%rax,%%cr3" ::
						  : "rax", "memory");
}
} // namespace arch::vmem
