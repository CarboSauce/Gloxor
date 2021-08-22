#include "pmm.hpp"

namespace glox
{
   // This should point to context initialized by early protocol inits
   glox::pmmHeader* pmmCtx;
   void* pmmAlloc(size_t pNumber)
   {
      return nullptr;
   }
   
   void pmmFree(void* ptr)
   {

   }
}