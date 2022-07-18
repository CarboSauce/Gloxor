#include "system/danger.hpp"
#include "glox/algo.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "arch/cpu.hpp"

static inline void drawPanic()
{

/* 	auto width = con.width;
	auto height = con.height;
	auto beg = con.fbBeg;
	auto hmmm = 0;
	for (size_t i = 0; i < height; ++i)
	{
		for (size_t j = 0; j < width; ++j)
		{
			hmmm = (hmmm + (i * width + j)) % 12321;
			beg[i * width + j] = hmmm;
		}
	} */
	gloxDebugLog("\nKernel Panic moment");

}

void glox::kernelPanic()
{
	drawPanic();
	arch::haltForever();
}



