#pragma once
#include "gloxor/types.hpp"
#include <compare>

namespace glox
{   
   struct pmmHeader
   {
      void* start; // Start of memory
      pmmHeader* next; // Next header in list
      size_t size; // Size of memory, bitmap size is 8*PAGE smaller
      u8 bitmap[];
      struct iterator
      {
         pmmHeader* it;
         auto operator++(){return it = it->next;}
         //auto operator!=(iterator other){return it != other.it;}
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
    */
   extern pmmHeader* pmmCtx;
   /**
    * @brief Allocate pNumber amount of pages 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc(size_t pNumber);
   /**
    * @brief Free the allocated page
    * @param ptr Pointer previously obtained from pmm::alloc
    */
   void pmmFree(void* ptr);
}