#pragma once
#include <asm/paging.hpp>
#include <glox/util.hpp>
#include <gloxor/types.hpp>
#define VMEM_MASK(a,b) ((u64)(a) | (u64)(b))
namespace arch::vmem
{
using paging_t = u64;
using vpage_flags = u64;
enum class PagePrivileges : u64
{
	readOnly = x86::vmem::noexec,								  // read
	writeOnly = x86::vmem::noexec | x86::vmem::writable, // write, on x86 implies readable
	execOnly = 0,													  // exec, on x86 exec implies readable
	readWrite = writeOnly,										  // read+write
	readExec = execOnly,											  // read+exec
	all = x86::vmem::writable,									  // read+write+exec
};
enum class PageCaching
{
	writeBack = 0,
	writeThrough = 1,
	cacheDisable = 2,
	writeCombine = 3
};
constexpr u64 pageSize = 0x1000;
static constexpr vpage_flags defFlags = x86::vmem::writable | x86::vmem::present;
/**
 * @brief class for managing virtual memory context
 *
 */
using vmemCtxT = u64[512];
/**
 * @brief Map virtual address to physical address
 *
 * @param from Virtual address to map from
 * @param to Physical address to map to
 * @return true Success
 * @return false Mapping failed
 */
bool map_huge_page(vmemCtxT, vaddrT from, paddrT to, vpage_flags flags = arch::vmem::defFlags);
bool map(vmemCtxT, vaddrT from, paddrT to, vpage_flags flags = arch::vmem::defFlags);
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
inline void set_context(vmemCtxT context)
{
	asm volatile("mov %0, %%cr3" ::"r"(mask_entry(get_real_address((u64)context), x86::vmem::writeThrough)));
}
void destroy_context(vmemCtxT context);

inline vmemCtxT* get_context()
{
	vmemCtxT* ctx;
	asm volatile("mov %%cr3,%0"
					 : "=r"(ctx));
	return ctx;
}
inline void flush_single(void* addr)
{
	asm volatile("invlpg (%0)" ::"r"(addr)
					 : "memory");
}
inline void flush_all()
{
	asm volatile("movq %%cr3, %%rax;mov %%rax,%%cr3" ::
						  : "rax", "memory");
}
} // namespace arch::vmem
