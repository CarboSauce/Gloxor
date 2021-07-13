#include "gloxor/mutex.hpp"
#include "arch/irq.hpp"
namespace glox
{
   irqMutex::irqMutex()
   {
      irqCtx = arch::saveIrq();
      arch::stopIrq();
   }

   irqMutex::~irqMutex()
   {
      arch::restoreIrq(irqCtx);
   }   
}