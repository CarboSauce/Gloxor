export module gloxor.mutex;
import glox;
import arch;
import arch.irq;

export namespace gx {
class IrqLock
{
    arch::irqT irqCtx;

public:
    IrqLock() = default;
    void lock();
    void unlock();
    IrqLock(IrqLock const&) = delete;
    IrqLock& operator=(IrqLock const&) = delete;
};

class IrqScopeLock
{
    gx::IrqLock l;
    glox::scoped_lock<IrqLock> m { l };
};

class IrqSpinLock
{
    gx::IrqLock l;
    glox::spinlock s;

public:
    void lock()
    {
        l.lock();
        s.lock();
    }

    void unlock()
    {
        s.unlock();
        l.unlock();
    }
};
}; // namespace gx
