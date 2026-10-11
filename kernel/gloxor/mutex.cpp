import arch;

namespace gx {
void IrqLock::lock()
{
    irqCtx = arch::save_irq();
    arch::stop_irq();
}

void IrqLock::unlock()
{
    arch::restore_irq(irqCtx);
}
} // namespace gx
