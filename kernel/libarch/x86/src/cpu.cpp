#include "arch/irq.hpp"
#include "asm/asmstubs.hpp"
#include "asm/gdt.hpp"
#include "asm/idt.hpp"
#include "debug.hpp"
#include "string.h"

using namespace arch;

static gdt code_data[3];
static idt idt_list[256]{};

[[gnu::interrupt]] static void DivZeroHandle(interrupt_frame_t* frame)
{
   print_debug("You Fool Divided By Zero!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void DoubleFault(interrupt_frame_t* frame)
{
   print_debug("Double fault!\n");
   while (1)
      ;
}
[[gnu::interrupt]] static void SpurInterrupt(interrupt_frame_t* frame)
{
   print_debug("Spurious Interrupt!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void GPfault(interrupt_frame_t* frame, size_t errc)
{
   print_debug("General Protection Fault!\n");
   while (1)
      ;
}

[[gnu::interrupt]] static void PageFault(interrupt_frame_t* frame, size_t errc)
{
   print_debug("Page Fault!\n");
   while (1)
      ;
}

void initializeGdt();
void initializeInterrupts();

namespace arch
{

   void initializeCpu()
   {
      initializeGdt();
      initializeInterrupts();
   }

} // namespace arch

void initializeGdt()
{
   gdt_pointer gdt_ptr = {
       sizeof(code_data),
       code_data};
   code_data[0] = {}; // null descriptor
   code_data[1] = {
       0x0000,     // limit
       0x0000,     // base
       0x00,       // base
       0b10011010, // flags
       0b00100000, // gran
       0x00        // base
   };
   code_data[2] = {
       0x0000,     // limit
       0x0000,     // base
       0x00,       // base
       0b10010010, // flags
       0b00000000, // gran
       0x00        // base
   };

   load_gdt(gdt_ptr);
   // Perform long jump after loading gdt to flush instruction cache
   // x86_64 doesnt support immediate long jump, so we have to do "magic"
   // Its really ugly but tldr; it loads long pointer on stack and iretqs
   asm volatile(
       ".intel_syntax noprefix;"
       "push rbp\n"
       "mov rbp, rsp\n"
       "push %0\n"
       "push rbp\n"
       "pushfq\n"
       "push %1\n"
       "push offset %=f\n"
       "iretq\n"
       "%=:\n"
       "pop rbp\n"
       "mov ds, %0\n mov es, %0\n mov fs, %0\n mov gs, %0\n mov ss, %0\n"
       ".att_syntax;"
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
   memset(idt_list, 0, sizeof(idt_list));

   idt_list[0].registerHandler((uint64_t)DivZeroHandle, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[2].registerHandler((uint64_t)SpurInterrupt, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[8].registerHandler((uint64_t)DoubleFault, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[13].registerHandler((uint64_t)GPfault, 0x8, 0, IDT_INTERRUPTGATE);
   idt_list[14].registerHandler((uint64_t)PageFault, 0x8, 0, IDT_INTERRUPTGATE);
   loadIdt(idt_ptr);

   stopIrq();
}
