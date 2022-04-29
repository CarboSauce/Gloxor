#pragma once
#include "gloxor/types.hpp"
#include <compare>

namespace glox
{   

   /**
    * @brief use this incase you need access to internals of pmm,  
    * its temporary workaround of not having proper initilalization functions    
    */
   //extern pmmHeader* pmmCtx;

   constexpr sizeT pmmChunkSize = 0x1000;
   /**
    * @brief use this after adding all memory chunks with pmmAddChunk
    */
   //void pmmFinalize();
   /**
    * @brief Adds memory chunk to PMM
    */
   void pmmAddChunk(void* base, size_t length);
   /**
    * @brief Allocate single page
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc();
   /**
    * @brief Allocate pageCount amount of pages 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc(size_t pageCount);
   /**
    * @brief Allocate single page below given address 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc(void* below);
   /**
    * @brief Allocate pageCount amount of pages below given address 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc(void* below, size_t pageCount);
   /**
    * @brief Allocate single page and zero it 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAllocZ();

   /**
    * @brief Free the allocated page
    * @param ptr Pointer previously obtained from pmm::alloc
    */
   //void pmmFree(void* ptr);
   void pmmFree(void* ptr,sizeT pageCount = 1);
}