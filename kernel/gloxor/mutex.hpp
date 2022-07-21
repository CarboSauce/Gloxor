#pragma once
#include "arch/irq.hpp"
#include "glox/mutex.hpp"

namespace glox
{
	class irqMutex
	{
		arch::irqT irqCtx;
		public:
		irqMutex() = default;
		void lock();
		void unlock();
		irqMutex(const irqMutex&) = delete;
		irqMutex& operator=(const irqMutex&) = delete;
	};
}; // namespace glox
