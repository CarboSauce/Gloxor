#pragma once
#include "gloxor/types.hpp"
#include <compare>
#if defined(__GNUC__) && !defined(__clang__)
	#define mallocAttribute(...) malloc(__VA_ARGS__)
#else
	#define mallocAttribute(...)
#endif

namespace glox
{
/**
 * @brief use this incase you need access to internals of pmm,
 * its temporary workaround of not having proper initilalization functions
 */
constexpr sizeT pmmChunkSize = 0x1000;
/**
 * @brief use this after adding all memory chunks with pmmAddChunk
 */
// void pmmFinalize();
/**
 * @brief Adds memory chunk to PMM
 */
void pmm_add_chunk(void* base, sizeT length);
/**
 * @brief Free the allocated page
 * @param ptr Pointer previously obtained from pmm::alloc
 */
void page_dealloc(void* ptr, sizeT pageCount = 1);
/**
 * @brief Allocate pageCount amount of pages
 * @return pointer to allocated page, nullptr on out of memory
 */
[[using gnu: malloc, mallocAttribute(glox::page_dealloc, 1), alloc_size(1), aligned(glox::pmmChunkSize)]]
void* page_alloc(sizeT pageCount = 1);
/**
 * @brief Allocate single page and zero it
 * @return pointer to allocated page, nullptr on out of memory
 */
[[using gnu: malloc, mallocAttribute(glox::page_dealloc, 1), alloc_size(1), aligned(glox::pmmChunkSize)]]
void* page_alloc_z(sizeT pageCount = 1);

} // namespace glox