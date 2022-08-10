#include "arch/cpu.hpp"
#include "arch/feature.hpp"
#include "arch/irq.hpp"
#include "asm/asmstubs.hpp"
#include "asm/gdt.hpp"
#include "asm/idt.hpp"
#include "gloxor/test.hpp"
#include "memory/virtmem.hpp"
#include "system/danger.hpp"
#include "system/logging.hpp"

/*
[[gnu::no_caller_saved_registers]] extern "C" void _gloxAsmLongJump(); */
using namespace arch;
using namespace glox;
using namespace arch::vmem;

[[gnu::used]] static gdt code_data[3]{
	 {},
	 {
		  0x0000,	  // limit
		  0x0000,	  // base
		  0x00,		  // base
		  0b10011010, // flags
		  0b00100000, // gran
		  0x00		  // base
	 },
	 {
		  0x0000,	  // limit
		  0x0000,	  // base
		  0x00,		  // base
		  0b10010010, // flags
		  0b00000000, // gran
		  0x00		  // base
	 }};
[[gnu::used]] static idt idt_list[256]{};
static u64 cpuFeatures;

[[gnu::interrupt]] static void DivZeroHandle(interrupt_frame_t*)
{
	gloxFatalLogln("You Fool Divided By Zero!\n");
	glox::kernelPanic();
}

[[gnu::interrupt]] static void DoubleFault(interrupt_frame_t*)
{
	gloxFatalLogln("Double fault!\n");
	glox::kernelPanic();
}
[[gnu::interrupt]] static void SpurInterrupt(interrupt_frame_t*)
{
	gloxFatalLogln("Spurious Interrupt!\n");
}

[[gnu::interrupt]] static void GPfault(interrupt_frame_t* frame, size_t /* errc */)
{
	gloxFatalLogln("General Protection Fault!\nRIP = ", (void*)frame->ip);
	glox::kernelPanic();
}

[[gnu::interrupt]] static void PageFault(interrupt_frame_t*, size_t /* errc */)
{
	void* errorAdr = (void*)readCr(2);
	gloxFatalLogln("Page Fault at address: ", errorAdr);
	if (errorAdr < (u8*)0x1000)
	{
		gloxTraceLog("Null pointer access\n");
	}
	glox::kernelPanic();
}

[[gnu::interrupt]] static void IllegalOpcode(interrupt_frame_t* frame)
{
	gloxFatalLogln("Illegal opcode! RIP = ", (void*)frame->ip);
	arch::haltForever();
}

[[gnu::interrupt]] static void DebugHandler(interrupt_frame_t* frame)
{
	gloxPrintln("Debug handler, RIP: ", (void*)frame->ip);
}

inline void initializeGdt();
inline void initializeInterrupts();
inline void initializeCpuExtensions();

namespace x86
{
vmemCtxT initKernelVirtMem();
}

namespace arch
{

bool isFeatureSupported(featureBit features)
{
	return cpuFeatures & static_cast<u64>(features);
}

void initializeCpu()
{
	initializeGdt();
	initializeInterrupts();
	initializeCpuExtensions();

	gloxTraceLogln("Cpu features:", cpuFeatures);
	x86::initKernelVirtMem();
}

} // namespace arch

inline void initializeGdt()
{
	stopIrq();

	gdt_pointer gdt_ptr = {
		 sizeof(code_data),
		 code_data};

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

inline void initializeInterrupts()
{

	idtPointer idt_ptr = {
		 sizeof(idt_list),
		 idt_list};

	idt_list[0].registerHandler((u64)DivZeroHandle, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[1].registerHandler((u64)SpurInterrupt, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[2].registerHandler((u64)SpurInterrupt, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[6].registerHandler((u64)IllegalOpcode, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[8].registerHandler((u64)DoubleFault, 0x8, 0, IDT_TRAPGATE);
	idt_list[13].registerHandler((u64)GPfault, 0x8, 0, IDT_INTERRUPTGATE);
	idt_list[14].registerHandler((u64)PageFault, 0x8, 0, IDT_INTERRUPTGATE);
	loadIdt(idt_ptr);
}

inline void initializeCpuExtensions()
{
	u32 eax, ebx, ecx, edx;
	__cpuid(0x1, eax, ebx, ecx, edx);
	bool isOSXsaveEnabled = 0;
	if (edx & (1 << 25))
	{
		u64 control;
		control = readCr(0);
		control &= ~(1 << 2);
		control |= 1 << 1;
		writeCr(0, control);
		control = readCr(4);
		control |= (1 << 9 | 1 << 10);
		cpuFeatures |= 1;
		if (ecx & (1 << 26))
		{
			control |= 1 << 18;
			isOSXsaveEnabled = true;
		}
		writeCr(4, control);
	}

	if ((ecx & (1 << 28)) && isOSXsaveEnabled)
	{
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

registerTest("Cpu flags",testing);
#endif
