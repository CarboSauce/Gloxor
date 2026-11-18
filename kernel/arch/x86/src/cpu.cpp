module arch.cpu;
import gloxor.system;
import gloxor.memory;
import arch;
import arch.percpu;
import arch_asm;
import gloxor.types;
import std.compat;
import arch_asm.msr;
import glox;

#include "cpuid.h"
#include "gloxor/test.hpp"

/*
[[gnu::no_caller_saved_registers]] extern "C" void _gloxAsmLongJump(); */
using namespace arch;
using namespace gx;
using namespace arch::vmem;

[[gnu::used]]
static Gdt code_data[3] = {
    { },
    {
        0x0000, // limit
        0x0000, // base
        0x00, // base
        0b10011010, // flags
        0b00100000, // gran
        0x00, // base
    },
    {
        0x0000, // limit
        0x0000, // base
        0x00, // base
        0b10010010, // flags
        0b00000000, // gran
        0x00, // base
    },
};
[[gnu::used]]
static Idt idt_list[256] { };
static u64 cpuFeatures;

extern "C" void int_handle(
    arch::InterruptRegs* regs,
    u64 intNumber,
    u64 errorCode
)
{
    switch (intNumber) {
    case 0: // Divide by zero
        gx::log_fatal("You Fool Divided By Zero!\r\n");
        gx::kernel_panic();
        break;

    case 1: // Debug
        gx::println("Debug handler, RIP: %", (void*)regs->rip);
        break;

    case 2: // NMI
        gx::log_fatal("Non Maskable Interrupt!\r\n");
        gx::kernel_panic();
        break;

    case 6: // Invalid opcode
        gx::log_fatal("Illegal opcode! RIP = %\r\n", (void*)regs->rip);
        arch::halt_forever();
        break;

    case 8: // Double fault
        gx::log_fatal("Double fault!\r\n");
        gx::kernel_panic();
        break;

    case 13: // General protection fault
        gx::log_fatal(
            "General Protection Fault!\r\n"
            "RIP = %\r\n"
            "Error code = %\r\n",
            (void*)regs->rip,
            errorCode
        );
        gx::kernel_panic();
        break;

    case 14: // Page fault
    {
        void* errorAddress = (void*)read_cr2();

        gx::log_fatal(
            "Page Fault!\r\n"
            "RIP = %\r\n"
            "Address = %\r\n"
            "Error code = %\r\n",
            (void*)regs->rip,
            errorAddress,
            errorCode
        );

        if ((uintptr_t)errorAddress < 0x1000) {
            gx::log_fatal("Null pointer access!\r\n");
        }

        gx::kernel_panic();
        break;
    }

    default:
        auto irq = gx::irq::handler_at(intNumber);
        if (irq) {
            irq.val().handler();
        } else {
            gx::log_fatal(
                "Unhandled interrupt % at RIP = %\r\n"
                "Error code = %\r\n",
                intNumber,
                (void*)regs->rip,
                errorCode
            );
            gx::kernel_panic();
        }
        break;
    }
}

inline void initialize_gdt();
inline void initialize_interrupts();
inline void initialize_cpu_extensions();

namespace arch {

bool is_feature_supported(FeatureBit features)
{
    return cpuFeatures & static_cast<u64>(features);
}

inline bool setup_pat()
{
    auto info = cpuid(1);
    if (!(info.edx & (1 << 16)))
        return false;
    // WC UC WT WB the CPU default
    gx::log_debug("Current ia32PAT is: %\r\n", (void*)rdmsr(msr::IA32_PAT));
    u32 patlow = 0x01000406;
    // UC UC- WP WC
    u32 pathigh = 0x00070105;
    wrmsr(msr::IA32_PAT, patlow, pathigh);
    return true;
}
void initialize_cpu()
{
    initialize_gdt();
    auto s = arch::percpu_area_size();
    auto buf = gx::PmmAllocator::alloc(s);
    arch::initialize_percpu(buf, s);
    initialize_interrupts();
    initialize_cpu_extensions();
    if (setup_pat())
        gx::log_debug("PAT supported on boot cpu\r\n");

    gx::log_debug("Cpu features: %\r\n", cpuFeatures);
}

} // namespace arch

inline void initialize_gdt()
{
    stop_irq();

    GdtPointer gdt_ptr = { sizeof(code_data) - 1, code_data };

    load_gdt(gdt_ptr);
    // Perform long jump after loading gdt to flush instruction cache
    // x86_64 doesnt support immediate long jump, so we have to do "magic"
    // Its really ugly but tldr; it loads long pointer on stack and iretqs
    asm volatile(
        "pushq %%rbp\r\n"
        "movq %%rsp, %%rbp\r\n"
        "push %0\r\n"
        "pushq %%rbp\r\n"
        "pushfq\r\n"
        "push %1\r\n"
#if (__PIE__ > 0) || (__PIC__ > 0)
        "leaq 1f(%%rip), %%rax\r\n" // 0x3 is the size of pushq and iretq
        "pushq %%rax\r\n"
#else
        "pushq $1f\r\n"
#endif
        "iretq\r\n"
        "1:\r\n"
        "pop %%rbp\r\n"
        "mov %0,%%ds; mov %0,%%es; mov %0,%%fs; mov %0,%%gs; mov %0,%%ss;"
        :
        : "r"((uint64_t)(0x10) /* ds */), "r"((uint64_t)0x8 /* cs */)
        : "memory", "%rax"
    );
}

extern "C" u64 intVector[256];
inline void initialize_interrupts()
{

    for (u64 i = 0; i < 256; ++i) {
        idt_list[i].register_handler(intVector[i], 0x8, 0, IDT_INTERRUPTGATE);
    }
    IdtPointer idtPtr = { sizeof(idt_list) - 1, idt_list };
    load_idt(idtPtr);
}

inline void initialize_cpu_extensions()
{
    u32 eax, ebx, ecx, edx;
    __cpuid(0x1, eax, ebx, ecx, edx);
    bool isOSXsaveEnabled = 0;
    if (edx & (1 << 25)) {
        u64 control;
        control = read_cr0();
        control &= ~(1 << 2);
        control |= 1 << 1;
        write_cr0(control);
        control = read_cr4();
        control |= (1 << 9 | 1 << 10);
        cpuFeatures |= 1;
        if (ecx & (1 << 26)) {
            control |= 1 << 18;
            isOSXsaveEnabled = true;
        }
        write_cr4(control);
    }

    if ((ecx & (1 << 28)) && isOSXsaveEnabled) {
        auto reg = xgetbv(0);
        reg.eax |= 7;
        xsetbv(0, reg.edx, reg.eax);
        cpuFeatures |= 1 << 1;
    }
}

#ifdef TEST
static bool testing()
{
    u32 eax, ebx, ecx, edx;
    __cpuid(0x1, eax, ebx, ecx, edx);
    gx::log_trace("CPUID ECX = %\r\n", ecx);
    gx::log_trace("Is SSE supported? %\r\n", (edx & (1 << 25)) != 0);
    gx::log_trace("Is XSAVE supported? %\r\n", (ecx & (1 << 26)) != 0);
    gx::log_trace("Is OSXSAVE supported? %\r\n", (ecx & (1 << 27)) != 0);
    gx::log_trace("Is RDRAND supported? %\r\n", ((ecx >> 30) & 1));
    gx::log_trace("Is AVX supported? %\r\n", (ecx & (1 << 28)) != 0);
    return true;
}

REGISTER_TEST("Cpu flags", testing);
#endif
