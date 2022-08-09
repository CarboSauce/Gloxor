#include "gloxor/mutex.hpp"
#include "arch/irq.hpp"
namespace glox
{
void irqLock::lock()
{
	irqCtx = arch::saveIrq();
	arch::stopIrq();
}

void irqLock::unlock()
{
	arch::restoreIrq(irqCtx);
}
} // namespace glox
