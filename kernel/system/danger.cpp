#include "system/danger.hpp"
#include "arch/cpu.hpp"
#include "system/logging.hpp"
static inline void drawPanic()
{
	glox::outStream, "\nKernel Panic moment";
}

void glox::kernelPanic()
{
	drawPanic();
	arch::haltForever();
}
