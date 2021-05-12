#include "asmstubs.h"
#include "gdt.hpp"
#include "idt.hpp"
#include "string.h"
#include "libglox/logger.hpp"
#include "libsane/debug.hpp"
static gdt code_data[3];
static gdt_pointer gdt_ptr;
static idt_pointer idt_ptr;
static idt idt_list[256]{};


static __attribute__ ((interrupt)) void DivZeroHandle (interrupt_frame_t* frame)
{
   print_debug("You Fool Divided By Zero!");
   while(1);

}

static __attribute__ ((interrupt)) void DoubleFault (interrupt_frame_t* frame)
{
   print_debug("Double fault!\n");
   while(1);

}
static __attribute__ ((interrupt)) void SpurInterrupt (interrupt_frame_t* frame)
{
   print_debug("Spurious Interrupt!\n");
   while(1);
}

static __attribute__ ((interrupt)) void GPfault(interrupt_frame_t* frame,size_t errc)
{
   print_debug("General Protection Fault!\n");
   while(1);
}

namespace arch
{
   void* get_idt()
   {
      return idt_list;
   }

   void initialize_gdt ()
   {
      gdt_ptr = {
         sizeof (code_data),
         code_data
      };
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

      stop_interrupts ();
      load_gdt (gdt_ptr);

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
          : "r"((uint64_t) (0x10) /* ds */), "r"((uint64_t)0x8 /* cs */)
          : "memory");
   }

   void initialize_interrupts ()
   {
      idt_ptr = {
         0xFFF,//sizeof (idt_list),
         idt_list
      };

      /* 
         Temporary, as my bootloader doesnt load ELF executables
         Once it does, this should be zeroed in BSS
      */
      memset(idt_list,sizeof(idt_list),0);

      load_idt (idt_ptr);
     
      idt_list[0].registerHandler((uint64_t)DivZeroHandle,0x8,0,IDT_INTERRUPTGATE);   
      idt_list[2].registerHandler((uint64_t)SpurInterrupt,0x8,0,IDT_INTERRUPTGATE);   
      idt_list[8].registerHandler((uint64_t)DoubleFault,0x8,0,IDT_INTERRUPTGATE);   
      idt_list[13].registerHandler((uint64_t)GPfault,0x8,0,IDT_INTERRUPTGATE);  

      start_interrupts();

      

   }

   void initialize_cpu ()
   {
      initialize_gdt ();
      initialize_interrupts ();
   }




} // namespace arch