#include "asm/asmstubs.hpp"
#include "system/logging.hpp"
#include "asm/idt.hpp"
#include "asm/pic8042contr.hpp"
#include "gloxor/modules.hpp"
#include "system/terminal.hpp"

[[gnu::interrupt]] static void kbdHandler(interrupt_frame_t* frame)
{
   auto val = inb(0x60);
   if (val == 185)
   {
      glox::term::clearScreen(0);
   }
   else if (val == 158)
   {
      gloxTraceLog("Borgir\n");
   }
   else if (val == 22)
      gloxTraceLog("Bor");
   else if (val == 150)
      gloxTraceLog("gar");
   else
      gloxTraceLogln("Kbval = ",val);
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
