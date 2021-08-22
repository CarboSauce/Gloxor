#include "arch/irq.hpp"
#include "arch/cpu.hpp"
#include "asm/asmstubs.hpp"
#include "asm/gdt.hpp"
#include "asm/idt.hpp"
#include "system/logging.hpp"
/* 
[[gnu::no_caller_saved_registers]] extern "C" void _gloxAsmLongJump(); */
using namespace arch;

[[gnu::used]] static gdt code_data[3]
{
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
[[gnu::used]] static idt idt_list[256]{};

[[gnu::interrupt]] static void DivZeroHandle(interrupt_frame_t* )
{

   gloxLogln("You Fool Divided By Zero!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void DoubleFault(interrupt_frame_t* )
{
   gloxLogln("Double fault!\n");
   while (1)
      ;
}
[[gnu::interrupt]] static void SpurInterrupt(interrupt_frame_t* )
{
   gloxLogln("Spurious Interrupt!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void GPfault(interrupt_frame_t* frame, size_t errc)
{
   gloxLog("General Protection Fault!\nRIP = ",(void*)frame->ip);
   while (1)
      ;
}

[[gnu::interrupt]] static void PageFault(interrupt_frame_t* , size_t errc)
{
   gloxLogln("Page Fault!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void IllegalOpcode(interrupt_frame_t* , size_t errc)
{
   gloxLog("Illegal opcode!\n");
   while (1)
      ;
}

void initializeGdt();
void initializeInterrupts();
extern "C" void gloxAsmInitBasics();
extern "C" void gloxAsmInitAvx();
namespace x86
{
   void initKernelVirtMem();
}

namespace arch
{

   void initializeCpu()
   {
      initializeGdt();
      initializeInterrupts();
      gloxAsmInitBasics();
      u32 eax,ebx,ecx,edx;
      __cpuid(0x1,eax,ebx,ecx,edx);
      gloxLogln("CPUID ECX = ",ecx);
      gloxLogln("Is XSAVE supported? ",(ecx & (1 << 26)) != 0);
      gloxLogln("Is OSXSAVE supported? ",(ecx & (1 << 27)) != 0);
      gloxLogln("Is RDRAND supported? ",((ecx >> 30) & 1));
      gloxLogln("Is AVX supported? ",(ecx & (1 << 28)) != 0);
      gloxAsmInitAvx();
      //x86::initKernelVirtMem();
   }

   void haltForever()
   {
      while(1)
      {
         stopIrq();
         halt();
      }
   }

} // namespace arch

void initializeGdt()
{
   stopIrq();
   
gdt_pointer gdt_ptr = {
       sizeof(code_data),
       code_data};


   load_gdt(gdt_ptr);
   // Perform long jump after loading gdt to flush instruction cache
   // x86_64 doesnt support immediate long jump, so we have to do "magic"
   // Its really ugly but tldr; it loads long pointer on stack and iretqs
   // asm volatile(
   //     ".intel_syntax noprefix;"
   //     "push rbp\n"
   //     "mov rbp, rsp\n"
   //     "push %0\n"
   //     "push rbp\n"
   //     "pushfq\n"
   //     "push %1\n"
   //     "push offset %=f\n"
   //     "iretq\n"
   //     "%=:\n"
   //     "pop rbp\n"
   //     "mov ds, %0\n mov es, %0\n mov fs, %0\n mov gs, %0\n mov ss, %0\n"
   //     ".att_syntax;"
   //     :
   //     : "r"((uint64_t)(0x10) /* ds */), "r"((uint64_t)0x8 /* cs */)
   //     : "memory");
 asm volatile(
       //".intel_syntax noprefix;"
       "pushq %%rbp\n"
       "movq %%rsp, %%rbp\n"
       "push %0\n"
       "pushq %%rbp\n"
       "pushfq\n"
       "push %1\n"
       "pushq $1f\n"
       "iretq\n"
       "1:\n"
       "pop %%rbp\n"
       "mov %0,%%ds; mov %0,%%es; mov %0,%%fs; mov %0,%%gs; mov %0,%%ss;"
       //".att_syntax;"
       :
       : "r"((uint64_t)(0x10) /* ds */), "r"((uint64_t)0x8 /* cs */)
       : "memory");


}

void initializeInterrupts()
{
   
idt_pointer idt_ptr = {
       sizeof(idt_list),
       idt_list};

   /* 
      Temporary, as my bootloader doesnt load ELF executables
      Once it does, this should be zeroed in BSS
   */
//   memset(idt_list, 0, sizeof(idt_list));

   idt_list[0].registerHandler((uint64_t)DivZeroHandle, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[2].registerHandler((uint64_t)SpurInterrupt, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[6].registerHandler((uint64_t)IllegalOpcode, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[8].registerHandler((uint64_t)DoubleFault, 0x8, 0, IDT_TRAPGATE);
   idt_list[13].registerHandler((uint64_t)GPfault, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[14].registerHandler((uint64_t)PageFault, 0x8, 0, IDT_INTERRUPTGATE);
   loadIdt(idt_ptr);

}
