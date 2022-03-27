#pragma once
#include "gloxor/types.hpp"
#include <compare>

namespace glox
{   
   struct pmmHeader
   {
      pmmHeader* next; // Next header in list
      pmmHeader* prev;
      size_t size; // Size of memory, bitmap size is 8*PAGE smaller
      struct iterator
      {
         pmmHeader* it;
         auto operator++(){return it = it->next;}
         auto operator--(){return it = it->prev;}
         friend auto operator<=>(iterator,iterator) = default;
         auto& operator*() const { return *it;}
      };
      iterator begin()
      {
         return {this};
      }
      iterator end()
      {
         return {nullptr};
      }
   };

   /**
    * @brief use this incase you need access to internals of pmm,  
    * its temporary workaround of not having proper initilalization functions    
    */
   extern pmmHeader* pmmCtx;

   /**
    * @brief use this after adding all memory chunks with pmmAddChunk
    */
   void pmmFinalize();
   /**
    * @brief Adds memory chunk to PMM
    */
   void pmmAddChunk(void* base, size_t length);
   /**
    * @brief Allocate pNumber amount of pages 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc();
   /**
    * @brief Allocate single page and zero it 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAllocZ();

   /**
    * @brief Free the allocated page
    * @param ptr Pointer previously obtained from pmm::alloc
    */
   bool pmmFree(void* ptr);
}