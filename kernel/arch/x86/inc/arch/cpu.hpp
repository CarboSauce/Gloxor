#pragma once

namespace arch
{

inline void spinloop_hint()
{
	asm("pause");
}

inline void halt()
{
	asm("hlt");
}

void initialize_cpu();

[[noreturn]] inline void halt_forever()
{
	while (1)
	{
		asm("cli;hlt");
	}
}

} // namespace arch
