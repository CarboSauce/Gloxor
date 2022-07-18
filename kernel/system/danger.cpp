#include "system/danger.hpp"
#include "arch/cpu.hpp"
#include "glox/algo.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"

static inline void drawPanic()
{
	glox::outStream ,"\nKernel Panic moment";
}

void glox::kernelPanic()
{
	drawPanic();
	arch::haltForever();
}
