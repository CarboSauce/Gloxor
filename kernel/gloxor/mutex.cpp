#include "gloxor/mutex.hpp"
#include "arch/irq.hpp"
namespace glox
{
void IrqLock::lock()
{
	irqCtx = arch::save_irq();
	arch::stop_irq();
}

void IrqLock::unlock()
{
	arch::restore_irq(irqCtx);
}
} // namespace glox
