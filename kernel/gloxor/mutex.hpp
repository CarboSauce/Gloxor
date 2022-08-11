#pragma once
#include "arch/irq.hpp"
#include "glox/mutex.hpp"

namespace glox
{
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
}; // namespace glox
