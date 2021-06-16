#include "ps2keyboard.hpp"
#include "asm/asmstubs.hpp"
#include "debug.hpp"
#include "glox/logger.hpp"
#include "asm/idt.hpp"
#include "irqcontroller.hpp"
#include "arch/irq.hpp"

[[gnu::interrupt]] static void kbdHandler(interrupt_frame_t* frame)
{
   print_debug("Kbval=");
   auto val = inb(0x60);
   glox::log_integer(print_debug,val);
   outb(0xE9,'\n');
   glox::pic::sendEoiMaster();
}

struct initPic
{
   initPic(u32 off1, u32 off2)
   {
      glox::pic::remap(off1, off2);
   }
};

static ps2keyboard instance;

ps2keyboard::ps2keyboard()
{
   volatile initPic _picInstance(0x20, 0x28);
   auto idt = getIdt();
   print_debug("Mapping handler\n");
   idt.base[0x21].registerHandler((uint64_t)kbdHandler,0x8,0,IDT_INTERRUPTGATE);

   outb(PIC1_DATA, 0b11111101);
   outb(PIC2_DATA, 0xFF);
   print_debug("Starting Interrupts\n");
   arch::startIrq();
}













