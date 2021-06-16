#pragma once
#include <glox/types.hpp>

namespace arch
{

   using irq_t = u64;

   inline irq_t saveIrq(void)
   {
      irq_t flags;
      asm volatile("pushf;pop %0"
                   : "=r"(flags)
                   :
                   : "memory");
      return flags;
   }

   inline void restoreIrq(irq_t flags)
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