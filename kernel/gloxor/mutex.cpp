#include "gloxor/mutex.hpp"
#include "arch/irq.hpp"
namespace glox
{
void irqMutex::lock()
{
    irqCtx = arch::saveIrq();
    arch::stopIrq();
}

void irqMutex::unlock()
{
    arch::restoreIrq(irqCtx);
}
} // namespace glox
