#pragma once
#include "arch/irq.hpp"
namespace glox
{
	struct irqMutex
	{
		arch::irqT irqCtx;
		irqMutex();
		~irqMutex();
		irqMutex(const irqMutex&) = delete;
		irqMutex& operator=(const irqMutex&) = delete;
		irqMutex(irqMutex&&) = delete;
		irqMutex& operator=(irqMutex&&) = delete;
	};
}; // namespace glox
