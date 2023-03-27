#include "arch/cpu.hpp"
#include "arch/feature.hpp"
#include "arch/irq.hpp"
#include "asm/asmstubs.hpp"
#include "asm/gdt.hpp"
#include "asm/idt.hpp"
#include "asm/msr.hpp"
#include "gloxor/test.hpp"
#include "memory/virtmem.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"

/*
[[gnu::no_caller_saved_registers]] extern "C" void _gloxAsmLongJump(); */
using namespace arch;
using namespace gx;
using namespace arch::vmem;

[[gnu::used]] static Gdt code_data[3] {
	{},
	{
     0x0000,     // limit
		0x0000,     // base
		0x00,       // base
		0b10011010, // flags
		0b00100000, // gran
		0x00        // base
	},
	{
     0x0000,     // limit
		0x0000,     // base
		0x00,       // base
		0b10010010, // flags
		0b00000000, // gran
		0x00        // base
	}
};
[[gnu::used]] static Idt idt_list[256] {};
static u64 cpuFeatures;

[[gnu::interrupt]] static void div_zero_handle(InterruptFrame*)
{
	gloxFatalLogln("You Fool Divided By Zero!\n");
	gx::kernel_panic();
}

[[gnu::interrupt]] static void double_fault(InterruptFrame*)
{
	gloxFatalLogln("Double fault!\n");
	gx::kernel_panic();
}
[[gnu::interrupt]] static void nmi_handler(InterruptFrame*)
{
	gloxFatalLogln("Non Maskable Interrupt!\n");
}

[[gnu::interrupt]] static void gp_fault(InterruptFrame* frame, size_t /* errc */)
{
	gloxFatalLogln("General Protection Fault!\nRIP = ", (void*)frame->ip);
	gx::kernel_panic();
}

[[gnu::interrupt]] static void page_fault(InterruptFrame*, size_t /* errc */)
{
	void* errorAdr = (void*)readCr(2);
	gloxFatalLogln("Page Fault at address: ", errorAdr);
	if (errorAdr < (u8*)0x1000) {
		gloxTraceLog("Null pointer access\n");
	}
	gx::kernel_panic();
}

[[gnu::interrupt]] static void illegal_opcode(InterruptFrame* frame)
{
	gloxFatalLogln("Illegal opcode! RIP = ", (void*)frame->ip);
	arch::halt_forever();
}

[[gnu::interrupt]] static void debug_handler(InterruptFrame* frame)
{
	gloxPrintln("Debug handler, RIP: ", (void*)frame->ip);
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
	gloxDebugLogln("Current ia32PAT is: ", (void*)rdmsr(msr::ia32PAT));
	u32 patlow = 0x01000406;
	// UC UC- WP WC
	u32 pathigh = 0x00070105;
	wrmsr(msr::ia32PAT, patlow, pathigh);
	return true;
}
void initialize_cpu()
{
	initialize_gdt();
	initialize_interrupts();
	initialize_cpu_extensions();
	if (setup_pat())
		gloxDebugLogln("PAT supported on boot cpu");

	gloxTraceLogln("Cpu features:", cpuFeatures);
}

} // namespace arch

inline void initialize_gdt()
{
	stop_irq();

	GdtPointer gdt_ptr = {
		sizeof(code_data),
		code_data
	};

	load_gdt(gdt_ptr);
	// Perform long jump after loading gdt to flush instruction cache
	// x86_64 doesnt support immediate long jump, so we have to do "magic"
	// Its really ugly but tldr; it loads long pointer on stack and iretqs
	asm volatile(
		"pushq %%rbp\n"
		"movq %%rsp, %%rbp\n"
		"push %0\n"
		"pushq %%rbp\n"
		"pushfq\n"
		"push %1\n"
#if (__PIE__ > 0) || (__PIC__ > 0)
		"leaq 0x3(%%rip), %%rax\n" // 0x3 is the size of pushq and iretq
		"pushq %%rax\n"
#else
		"pushq $1f\n"
#endif
		"iretq\n"
		"1:\n"
		"pop %%rbp\n"
		"mov %0,%%ds; mov %0,%%es; mov %0,%%fs; mov %0,%%gs; mov %0,%%ss;"
		:
		: "r"((uint64_t)(0x10) /* ds */), "r"((uint64_t)0x8 /* cs */)
		: "memory", "%rax");
}

inline void initialize_interrupts()
{

	IdtPointer idt_ptr = {
		sizeof(idt_list),
		idt_list
	};

	idt_list[0].register_handler((u64)div_zero_handle, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[1].register_handler((u64)debug_handler, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[2].register_handler((u64)nmi_handler, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[6].register_handler((u64)illegal_opcode, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[8].register_handler((u64)double_fault, 0x8, 0, IDT_TRAPGATE);
	idt_list[13].register_handler((u64)gp_fault, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[14].register_handler((u64)page_fault, 0x8, 0, IDT_INTERRUPTGATE);
	load_idt(idt_ptr);
}

inline void initialize_cpu_extensions()
{
	u32 eax, ebx, ecx, edx;
	__cpuid(0x1, eax, ebx, ecx, edx);
	bool isOSXsaveEnabled = 0;
	if (edx & (1 << 25)) {
		u64 control;
		control = readCr(0);
		control &= ~(1 << 2);
		control |= 1 << 1;
		writeCr(0, control);
		control = readCr(4);
		control |= (1 << 9 | 1 << 10);
		cpuFeatures |= 1;
		if (ecx & (1 << 26)) {
			control |= 1 << 18;
			isOSXsaveEnabled = true;
		}
		writeCr(4, control);
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
	gloxTraceLogln("CPUID ECX = ", ecx);
	gloxTraceLogln("Is SSE supported? ", (edx & (1 << 25)) != 0);
	gloxTraceLogln("Is XSAVE supported? ", (ecx & (1 << 26)) != 0);
	gloxTraceLogln("Is OSXSAVE supported? ", (ecx & (1 << 27)) != 0);
	gloxTraceLogln("Is RDRAND supported? ", ((ecx >> 30) & 1));
	gloxTraceLogln("Is AVX supported? ", (ecx & (1 << 28)) != 0);
	return true;
}

registerTest("Cpu flags", testing);
#endif
