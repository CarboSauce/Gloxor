#pragma once
#include "arch/paging.hpp"
#include "gloxor/types.hpp"

namespace virt
{
	/**
	 * @brief class for managing virtual memory context
	 *
	 */
	using virtCtxT = arch::pagingT;

	/**
	 * @brief Map virtual address to physical address
	 *
	 * @param from Virtual address to map from
	 * @param to Physical address to map to
	 * @return true Success
	 * @return false Mapping failed
	 */
	bool map(virtCtxT, const void* from, const void* to);
	bool map(virtCtxT, const void* from, const void* to, u64 flags);
	/**
	 * @brief Unmap virtual address from current context
	 *
	 * @param whichVirtual which virtual address to unamp
	 * @return true Success
	 * @return false Unmapping failed, if even possible
	 */
	bool unmap(virtCtxT, const void* whichVirtual);
	/**
	 * @brief Translate virtual address to physical address
	 *
	 * @param from Virtual address to translate
	 * @return void* Physical Address from translation
	 */
	void* translate(const virtCtxT, const void* from);
	/**
	 * @brief Initialize Virtual memory context
	 * @note Possibly allocating, hence we need to return error
	 * @note I'd like it to be a constructor, but we dont have exception handling to propagate error
	 * @return true Success
	 * @return false Allocation failure
	 */
	bool virtInitContext(virtCtxT);
   void setContext(virtCtxT);
   void virtFlush(void* addr);
	void virtCacheFlush();
} // namespace arch