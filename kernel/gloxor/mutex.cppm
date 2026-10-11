export module gloxor : mutex;
import glox;
import arch;

export namespace gx {
class IrqLock
{
    arch::irqT irqCtx;

public:
    IrqLock() = default;
    void lock();
    void unlock();
    IrqLock(const IrqLock&) = delete;
    IrqLock& operator=(const IrqLock&) = delete;
};
}; // namespace gx
