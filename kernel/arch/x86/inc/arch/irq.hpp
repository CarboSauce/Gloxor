#pragma once
#include <gloxor/types.hpp>

namespace arch
{

   using irqT = u64;

   inline irqT saveIrq(void)
   {
      irqT flags;
      asm volatile("pushf;pop %0"
                   : "=rm"(flags)
                   :
                   : "memory");
      return flags;
   }

   inline void restoreIrq(irqT flags)
   {
      asm("push %0;popf"
          :
          : "rm"(flags)
          : "memory", "cc");
   }
   inline void stopIrq()
   {
      asm volatile("cli" ::
                       : "memory");
   }

   inline void startIrq()
   {
      asm volatile("sti" ::
                       : "memory");
   }
} // namespace arch