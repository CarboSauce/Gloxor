#include "irqcontroller.hpp"
#include "asm/asmstubs.hpp"
#include "debug.hpp"




namespace glox::pic
{
   void sendEoiSlave()
   {
      outb(PIC2_COMMAND, PIC_EOI);
   }
   void sendEoiMaster()
   {
      outb(PIC1_COMMAND, PIC_EOI);
   }



   void remap(u8 offset1, u8 offset2)
   {
      unsigned char a1, a2;

      a1 = inb(PIC1_DATA); // save masks
      a2 = inb(PIC2_DATA);

      outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
      ioWait();
      outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
      ioWait();
      outb(PIC1_DATA, offset1); // ICW2: Master PIC vector offset
      ioWait();
      outb(PIC2_DATA, offset2); // ICW2: Slave PIC vector offset
      ioWait();
      outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
      ioWait();
      outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)
      ioWait();

      outb(PIC1_DATA, ICW4_8086);
      ioWait();
      outb(PIC2_DATA, ICW4_8086);
      ioWait();
      outb(PIC1_DATA, a1); // restore saved masks.
      outb(PIC2_DATA, a2);
   }

} // namespace glox::pic
