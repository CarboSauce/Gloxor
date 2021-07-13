#include "ps2keyboard.hpp"
#include "asm/asmstubs.hpp"
#include "system/logging.hpp"
#include "asm/idt.hpp"
#include "pic8042contr.hpp"
#include "arch/irq.hpp"
#include "gloxor/modules.hpp"


[[gnu::interrupt]] static void kbdHandler(interrupt_frame_t* frame)
{
   gloxLogln("Kbval = ",inb(0x60));
   glox::pic::sendEoiMaster();
}

static void initKeyboard()
{
   auto idt = getIdt();
   gloxDebugLog("Mapping Keyboard handler\n");
   idt.base[0x21].registerHandler((uint64_t)kbdHandler,0x8,0,IDT_INTERRUPTGATE);

   // This should be generic, seperated drivers might have trouble to
   // unmask the controller
   glox::pic::setMasterMask(0b11111101);


}

initDriverModule(initKeyboard);
