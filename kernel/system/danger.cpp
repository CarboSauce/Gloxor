#include "system/danger.hpp"
#include "arch/cpu.hpp"
#include "system/logging.hpp"
static inline void draw_panic()
{
	glox::outStream, "\nKernel Panic moment";
}

void glox::kernel_panic()
{
	draw_panic();
	arch::halt_forever();
}
