#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "cpuid.h"
#include "gloxor/kinfo.hpp"
#include "gloxor/modules.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"

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
	gloxDebugLogln("Pre Cpu Init:");

	callCtorPointers(_modulePreCpuBegin, _moduleDriverCentralBegin);
}

extern "C" void callDriverInits()
{
	gloxDebugLogln("Driver Init:");
	callCtorPointers(_moduleDriverCentralBegin, _moduleDriverEnd);
	// We assume that Drivers havent enabled interrupts hopefully
	gloxDebugLog("Starting Interrupts after driver initialization\n");
	arch::startIrq();
}

extern "C" void callGlobalCtors()
{
	gloxDebugLogln("Global ctors :");
	callCtorPointers(_ctorArrayStart, _ctorArrayEnd);
}

// extern void sleep(u64 ticks, u64 ms);
// extern u64 getTicks();
extern "C" void gloxorMain()
{
	auto fbrange = glox::term::getUsedMemoryRange();
	gloxDebugLogln("Con begin: ", fbrange.begin());
	gloxDebugLogln("Con end: ", fbrange.end());
	callPreCpuInits();
	initializeCpu();
	callDriverInits();
	callGlobalCtors();
#ifdef GLOXTESTING
	extern ctor_t _moduleTesting[];
	extern ctor_t _moduleTestingEnd[];
	gloxTraceLog("Testing ctors: ");
	callCtorPointers(_moduleTesting, _moduleTestingEnd);
#endif
	gloxUnreachable();
}

static void gogole_test()
{
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
