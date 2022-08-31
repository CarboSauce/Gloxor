#include "arch/cpu.hpp"
#include "arch/irq.hpp"
#include "arch/addrspace.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"
#include "gloxor/modules.hpp"
#include "gloxor/test.hpp"
#include "gloxor/kinfo.hpp"
#include "memory/virtmem.hpp"
#include "cpuid.h"

using ctor_t = void (*)();
using namespace arch;
using namespace glox;
using namespace arch::vmem;
extern ctor_t _ctorArrayStart[];
extern ctor_t _ctorArrayEnd[];
extern ctor_t _modulePreCpuBegin[];
extern ctor_t _moduleDriverBegin[];
extern ctor_t _moduleDriverEnd[];

extern "C" void call_ctor_pointers(ctor_t* begin, ctor_t* end)
{
	for (auto it = begin; it != end; ++it)
	{
		(*it)();
	}
}

extern "C" void call_pre_cpu_inits()
{
	gloxDebugLogln("Pre Cpu Init:");
	call_ctor_pointers(_modulePreCpuBegin, _moduleDriverBegin);
}

extern "C" void call_driver_inits()
{
	gloxDebugLogln("Driver Init:");
	call_ctor_pointers(_moduleDriverBegin, _moduleDriverEnd);
	// We assume that Drivers havent enabled interrupts hopefully
	gloxDebugLog("Starting Interrupts after driver initialization\n");
	arch::start_irq();
}

extern "C" void call_global_ctors()
{
	gloxDebugLogln("Global ctors :");
	call_ctor_pointers(_ctorArrayStart, _ctorArrayEnd);
}

// extern void sleep(u64 ticks, u64 ms);
// extern u64 getTicks();
inline void map_region(vaddrT from, vaddrT to, paddrT start, arch::vmem::PagePrivileges pp, arch::vmem::PageCaching pc)
{
	auto isalign = [](auto a, auto b)
	{ return a % b == 0; };

	while (from < to)
	{

		if (to - from > 0x200'000 && isalign(from, 0x200'000))
		{
			kAddrSpace.map_huge(from, start, pp, pc);
			from += 0x200'000;
			start += 0x200'000;
		}
		else
		{
			kAddrSpace.map(from, start, pp, pc);
			from += 0x1000;
			start += 0x1000;
		}
	}
}
inline void map_kernel()
{
	size_t size = (kernelFileEnd - kernelFileBegin);
	gloxDebugLogln("kernelPhysOffset: ", kernelPhysOffset);
	for (size_t i = 0; i < size; i += pageSize)
	{
		kAddrSpace.map((vaddrT)kernelFileBegin + i, kernelPhysOffset + i, PagePrivileges::all, PageCaching::writeThrough);
	}
}
inline void identity_map()
{
	for (const auto& it : glox::machineInfo.mmapEntries)
	{
		if (it.type == BootInfo::MemTypes::usable || it.type == BootInfo::MemTypes::reclaimable)
		{
			const auto from = it.base + physicalMemBase;
			const auto to = it.base;
			map_region(from, from + it.length, to, PagePrivileges::all, PageCaching::writeThrough);
		}
	}
}

void init_addr_space()
{
	gloxDebugLogln("Remapping CR3 to ", (void*)&kAddrSpace);
	identity_map();
	auto [fbeg, fend] = glox::term::get_used_memory_range();
	gloxDebugLogln("Mapping framebuffer from: ", fbeg, " to: ", fend);
	map_region((vaddrT)fbeg, (paddrT)fend, get_real_data_addr((paddrT)fbeg), PagePrivileges::readWrite, PageCaching::writeCombine);
	map_kernel();
	gloxDebugLogln("Trying translation code, from : ", fbeg, " to: ", (void*)kAddrSpace.translate((u64)fbeg));
	gloxDebugLogln("Trying translation code, from : ", (u8*)physicalMemBase + 0x200'000, " to: ", (void*)kAddrSpace.translate(physicalMemBase + 0x200'000));
	// lets goo
}
extern "C" void gloxor_main()
{
	auto fbrange = glox::term::get_used_memory_range();
	gloxDebugLogln("Con begin: ", fbrange.begin());
	gloxDebugLogln("Con end: ", fbrange.end());
	call_pre_cpu_inits();
	initialize_cpu();
	init_addr_space();
	call_driver_inits();
	call_global_ctors();
#ifdef GLOXTESTING
	extern glox::Ktest _moduleTesting[];
	extern glox::Ktest _moduleTestingEnd[];
	glox::term::set_fg_color(0xadd8e6);
	gloxPrint("Unit tests:\nThere are ", _moduleTestingEnd - _moduleTesting, " tests\n");
	glox::term::set_fg_color(0xFFFFFF);
	for (auto it = _moduleTesting; it != _moduleTestingEnd; ++it)
	{
		gloxPrintln("Test case ", it->name);
		if (it->init())
		{
			glox::term::set_fg_color(0x00FF00);
			gloxPrint("Test passed\n");
		}
		else
		{
			glox::term::set_fg_color(0xFF0000);
			gloxPrint("Test failed\n");
		}
		glox::term::set_fg_color(0xFFFFFF);
	}
#endif
	gloxPrint("Initialization Completed.\n");
	for (;;)
		arch::halt();
}

#ifdef TEST
static bool gogole_test()
{
	u32 brand[12];
	__get_cpuid(0x80000002, brand + 0x0, brand + 0x1, brand + 0x2, brand + 0x3);
	__get_cpuid(0x80000003, brand + 0x4, brand + 0x5, brand + 0x6, brand + 0x7);
	__get_cpuid(0x80000004, brand + 0x8, brand + 0x9, brand + 0xa, brand + 0xb);
	glox::term::set_fg_color(0x80c000);
	glox::term::write_str((const char*)brand, 12 * sizeof(u32));
	glox::term::write_str("\n", 1);
	glox::term::set_fg_color(0xFFFFFF);
	return true;
}

registerTest("Stub", gogole_test);
#endif
