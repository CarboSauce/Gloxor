#pragma once
#include "arch/irq.hpp"
#include "glox/mutex.hpp"

namespace glox
{
class irqLock
{
	arch::irqT irqCtx;

 public:
	irqLock() = default;
	void lock();
	void unlock();
	irqLock(const irqLock&) = delete;
	irqLock& operator=(const irqLock&) = delete;
};
}; // namespace glox
