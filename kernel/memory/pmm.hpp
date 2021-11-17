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
      size_t bitmapSize;
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


   glox::pmmHeader* initPmm(void* base, size_t length, glox::pmmHeader* ctx);

   /**
    * @brief use this incase you need access to internals of pmm,  
    * its temporary workaround of not having proper initilalization functions    
    */
   extern pmmHeader* pmmCtx;
   /**
    * @brief Allocate pNumber amount of pages 
    * @return pointer to allocated page, nullptr on out of memory
    */
   void* pmmAlloc();
   /**
    * @brief Free the allocated page
    * @param ptr Pointer previously obtained from pmm::alloc
    */
   bool pmmFree(void* ptr);
}