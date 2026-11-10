import std;
import glox;
import gloxor.system;
import gloxor.memory;
import gloxor.types;
import gloxor.kinfo;
import arch;
import arch.irq;
import arch.cpu;
import arch.addrspace;
import std.compat;

#include "cpuid.h"
#include "glox/assert.hpp"
#include "gloxor/test.hpp"
#include "system/logging.hpp"

using ctor_t = void (*)();
using namespace arch;
using namespace gx;
using namespace arch::vmem;
extern ctor_t _ctorArrayStart[];
extern ctor_t _ctorArrayEnd[];
extern ctor_t _modulePreCpuBegin[];
extern ctor_t _moduleDriverBegin[];
extern ctor_t _moduleDriverEnd[];
extern gx::Ktest _moduleTesting[];
extern gx::Ktest _moduleTestingEnd[];

extern "C" void call_ctor_pointers(ctor_t* begin, ctor_t* end)
{
    for (auto it = begin; it != end; ++it) {
        (*it)();
    }
}

extern "C" void call_pre_cpu_inits()
{
    gx::logln_debug("Pre Cpu Init:");
    call_ctor_pointers(_modulePreCpuBegin, _moduleDriverBegin);
}

extern "C" void call_driver_inits()
{
    gx::logln_debug("Driver Init:");
    call_ctor_pointers(_moduleDriverBegin, _moduleDriverEnd);
    // We assume that Drivers havent enabled interrupts hopefully
    gx::logln_debug("Starting Interrupts after driver initialization\r\n");
    arch::start_irq();
}

extern "C" void call_global_ctors()
{
    gx::logln_debug("Global ctors :");
    call_ctor_pointers(_ctorArrayStart, _ctorArrayEnd);
}

// extern void sleep(u64 ticks, u64 ms);
// extern u64 getTicks();
inline void map_region(
    vaddr from,
    vaddr to,
    vaddr start,
    arch::vmem::PagePrivileges pp,
    arch::vmem::PageCaching pc
)
{
    auto isalign = [](auto a, auto b) { return a % b == 0; };

    while (from < to) {
        if (to - from > 0x200'000 && isalign(from, 0x200'000)) {
            kAddrSpace.map_huge(from, start, pp, pc);
            from += 0x200'000;
            start += 0x200'000;
        } else {
            kAddrSpace.map(from, start, pp, pc);
            from += 0x1000;
            start += 0x1000;
        }
    }
}
inline void map_kernel()
{
    size_t size = (kernelFileEnd - kernelFileBegin);
    gx::logln_debug("kernelPhysOffset: %", kernelPhysOffset);
    for (size_t i = 0; i < size; i += pageSize) {
        kAddrSpace.map(
            (vaddr)kernelFileBegin + i,
            kernelPhysOffset + i,
            PagePrivileges::all,
            PageCaching::writeThrough
        );
    }
}
inline void identity_map()
{
    for (const auto& it : gx::machineInfo.mmapEntries) {
        if (it.type == BootInfo::MemTypes::usable
            || it.type == BootInfo::MemTypes::reclaimable) {
            const auto from = it.base + physicalMemBase;
            const auto to = it.base;
            map_region(
                from,
                from + it.length,
                to,
                PagePrivileges::all,
                PageCaching::writeThrough
            );
        }
    }
}

void init_addr_space()
{
    gx::logln_debug("Remapping CR3 to %", &kAddrSpace);
    identity_map();
    auto [fbeg, fend] = gx::term::get_used_memory_range();
    gx::logln_debug("Mapping framebuffer from: % to: %", fbeg, fend);
    map_region(
        (vaddr)fbeg,
        (vaddr)fend,
        get_real_data_addr((vaddr)fbeg),
        PagePrivileges::readWrite,
        PageCaching::writeCombine
    );
    map_kernel();
    gx::logln_debug(
        "Trying translation code, from : % to %x",
        fbeg,
        kAddrSpace.translate((u64)fbeg)
    );
    gx::logln_debug(
        "Trying translation code, from : % to %x",
        (u8*)physicalMemBase + 0x200'000,
        kAddrSpace.translate(physicalMemBase + 0x200'000)
    );
    // lets goo
}
extern "C" void gloxor_main()
{
    auto fbrange = gx::term::get_used_memory_range();
    gx::logln_debug("Con begin: %", fbrange.begin());
    gx::logln_debug("Con end: %", fbrange.end());
    call_pre_cpu_inits();
    initialize_cpu();
    init_addr_space();
    call_driver_inits();
    call_global_ctors();
#ifdef GLOXTESTING
    gx::println(
        "Unit tests:\r\nThere are % tests", _moduleTestingEnd - _moduleTesting
    );
    for (auto it = _moduleTesting; it != _moduleTestingEnd; ++it) {
        gx::println("Test case %", it->name);
        if (it->init()) {
            gx::print("\e[0;32m");
            gx::print("Test passed\r\n");
        } else {
            gx::print("\e[0;31m");
            gx::print("Test failed\r\n");
        }
        gx::print("\e[0;37m");
    }
#endif
    gx::print("Initialization Completed.\r\n");
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
    gx::println("%", std::string_view { (const char*)brand, 12 });
    return true;
}

registerTest("Stub", gogole_test);
#endif
