#pragma once
#include <gloxor/types.hpp>

namespace arch
{

using irqT = u64;

inline irqT save_irq()
{
	irqT flags;
	asm volatile("pushf;pop %0"
					 : "=rm"(flags)
					 :
					 : "memory");
	return flags;
}

inline void restore_irq(irqT flags)
{
	asm("push %0;popf"
		 :
		 : "rm"(flags)
		 : "memory", "cc");
}
inline void stop_irq()
{
	asm volatile("cli" ::
						  : "memory");
}

inline void start_irq()
{
	asm volatile("sti" ::
						  : "memory");
}
} // namespace arch