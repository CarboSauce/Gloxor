#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "glox/algo.hpp"
#include "gloxor/graphics.hpp"
#include "gloxor/modules.hpp"
#include "protos/egg.h"
#include "system/danger.hpp"
#include "system/logging.hpp"
#include "memory/pmm.hpp"

using ctor_t = void (*)(void);
using namespace arch;
using namespace glox;
extern ctor_t _ctorArrayStart[];
extern ctor_t _ctorArrayEnd[];
extern ctor_t _modulePreCpuBegin[];
extern ctor_t _moduleDriverCentralBegin[];
extern ctor_t _moduleDriverEnd[];

extern "C" void callCtorPointers(ctor_t* begin, ctor_t* end)
{

	gloxLog("There are: ", static_cast<glox::u32>(end - begin), " Ctors\n");
	for (auto it = begin; it != end; ++it)
	{
		(*it)();
	}
}

extern "C" void callPreCpuInits()
{

	gloxLogln("Pre Cpu Init:");

	callCtorPointers(_modulePreCpuBegin, _moduleDriverCentralBegin);
}

extern "C" void callDriverInits()
{
	gloxLogln("Driver Init:");
	callCtorPointers(_moduleDriverCentralBegin, _moduleDriverEnd);
	// We assume that Drivers havent enabled interrupts hopefully
	gloxDebugLog("Starting Interrupts after driver initialization\n");
	arch::startIrq();
}

extern "C" void callGlobalCtors()
{
	gloxLogln("Global ctors :");
	callCtorPointers(_ctorArrayStart, _ctorArrayEnd);
}

void gogole_test(eggHandle*);
extern void sleep(u64 ticks, u64 ms);
extern u64 getTicks();
glox::framebuffer con;
extern "C" void gloxorMain()
{
	con = {

	(glox::rgb_t*)eggFrame.fb.fb_start,
	(glox::rgb_t*)eggFrame.fb.fb_end,
	eggFrame.fb.pitch,
	eggFrame.fb.width,
	eggFrame.fb.height,
	0xFFFFFF //white color
};
	callPreCpuInits();
	initializeCpu();
	callDriverInits();
	callGlobalCtors();
#ifdef GLOXTESTING
	extern ctor_t _moduleTesting[];
	extern ctor_t _moduleTestingEnd[];
	callCtorPointers(_moduleTesting, _moduleTestingEnd);
#endif

	gloxUnreachable();
}

void logFrameBuffer()
{
	gloxLogln("Frame Buffer Begin:\t", con.fbBeg);
	gloxLogln("Frame Buffer End:\t", con.fbEnd);
	gloxLogln("Frame Buffer Height:\t", con.height);
	gloxLogln("Frame Buffer Width:\t", con.width);
	gloxLogln("Frame Buffer Pitch:\t", con.pitch);
}

// Super useless and unscientific tests, take with grain of salt
void gogole_test()
{

	con.cls(0x101010);
	logFrameBuffer();
	gloxLogln("Memory map moment");

	size_t accum = 0;
	int i = 0;
/* 	for(auto it : *glox::pmmCtx)
	{
		accum += it.size;
	} */

	for (size_t i = 0; i < eggFrame.memMap.size; ++i)
	{
		gloxLogln("From ",(void*)eggFrame.memMap.base[i].base," to ",
		 (void*)(eggFrame.memMap.base[i].base+eggFrame.memMap.base[i].length),
		 " Value = ", eggFrame.memMap.base[i].type);
		
	}

	for (auto* it = pmmCtx; it != nullptr; it = it->next)
	{
		size_t s = it->size;
		gloxLogln("from it: ",it," size: ",it+s/sizeof(decltype(*it))," it->next: ",it->next);
		accum += s;++i;
	}

	gloxLogln("Iteration count is ",i,
	"\npmmCtx address and next val ",pmmCtx,' ',pmmCtx->next);
	gloxLogln("Size of memory is ",accum);
	
	// Invoking strlen doesnt work if strlen is a function defined as __builtin_strlen
/* 	u64 ticker = 0;
	char buff[30];
	while (1)
	{
		auto _tik = getTicks();
		buff[glox::format(buff, ticker)] = '\0';
		con.writeString(buff);
		con.curX = 0;
		con.curY = 0;
		sleep(_tik, 1000);
		ticker += 1;
	} */
	for (;;);
}

registerTest(gogole_test);