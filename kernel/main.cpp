#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "cpuid.h"
#include "gloxor/modules.hpp"
#include "memory/pmm.hpp"
#include "protos/egg.h"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "gloxor/graphics.hpp"
#include "asm/asmstubs.hpp"
using ctor_t = void (*)();
using namespace arch;
using namespace glox;
extern ctor_t _ctorArrayStart[];
extern ctor_t _ctorArrayEnd[];
extern ctor_t _modulePreCpuBegin[];
extern ctor_t _moduleDriverCentralBegin[];
extern ctor_t _moduleDriverEnd[];

extern "C" void callCtorPointers(ctor_t* begin, ctor_t* end)
{

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
//extern void sleep(u64 ticks, u64 ms);
//extern u64 getTicks();
extern "C" void gloxorMain()
{
	auto fbrange = glox::term::getUsedMemoryRange();
	gloxDebugLogln("Con begin: ",fbrange.begin());
	gloxDebugLogln("Con end: ",fbrange.end());
	callPreCpuInits();
	initializeCpu();
	callDriverInits();
	callGlobalCtors();
#ifdef GLOXTESTING
	extern ctor_t _moduleTesting[];
	extern ctor_t _moduleTestingEnd[];
	gloxLog("Testing ctors: ");
	callCtorPointers(_moduleTesting, _moduleTestingEnd);
#endif
	gloxUnreachable();
}

/* void logFrameBuffer()
{
	gloxLogln("Frame Buffer Begin:\t", con.fbBeg);
	gloxLogln("Frame Buffer End:\t", con.fbEnd);
	gloxLogln("Frame Buffer Height:\t", con.height);
	gloxLogln("Frame Buffer Width:\t", con.width);
	gloxLogln("Frame Buffer Pitch:\t", con.pitch);
}
 */


static void gogole_test()
{
	// glox::term::clearScreen(0x111111);

	size_t accum = 0;
	int i = 0;
	// for (auto& it : *glox::pmmCtx)
	// {
	// 	size_t s = it.size;
	// 	gloxLogln("Base of it: ", &it, " from it: ", it.start, " to: ", (void*)((u64)&it + s), " it->next: ", it.next,
	// 	" size: ",it.bitmapSize*8," number of pages ", s/4096); 
	// 	accum += s;
	// 	++i;
	// }


	gloxLogln("Iteration count is ", i,
			  "\npmmCtx address and next val ", pmmCtx, ' ', pmmCtx->next);
	gloxLogln("Size of memory is ", accum);
	// Invoking strlen doesnt work if strlen is a function defined as __builtin_strlen
	u32 brand[12];
	__get_cpuid(0x80000002, brand + 0x0, brand + 0x1, brand + 0x2, brand + 0x3);
	__get_cpuid(0x80000003, brand + 0x4, brand + 0x5, brand + 0x6, brand + 0x7);
	__get_cpuid(0x80000004, brand + 0x8, brand + 0x9, brand + 0xa, brand + 0xb);
	glox::term::setFgColor(0x80c000);
	glox::term::writeStr((const char*)brand, 12 * sizeof(u32));
	glox::term::writeStr("\n", 1);
	glox::term::setFgColor(0xFFFFFF);

}

registerTest(gogole_test);
