#include "asm/asmstubs.hpp"
#include "asm/idt.hpp"
#include "asm/pic8042contr.hpp"
#include "gloxor/modules.hpp"
#include "system/logging.hpp"
#include "system/terminal.hpp"

[[gnu::interrupt]] static void kbd_handler(InterruptFrame* frame)
{
	auto val = inb(0x60);
	if (val == 185)
	{
		glox::term::clear_screen(0); // i think this basically can crash kernel, too complex in irq
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
		gloxTraceLogln("Kbval = ", val);
	glox::pic::send_eoi_master();
}

static void init_keyboard()
{
	auto idt = get_idt();
	gloxDebugLog("Mapping Keyboard handler\n");
	idt.base[0x21].register_handler((uint64_t)kbd_handler, 0x8, 0, IDT_INTERRUPTGATE);

	// This should be generic, seperated drivers might have trouble to
	// unmask the controller
	glox::pic::set_master_mask(0b11111101);
}

initDriverModule(init_keyboard);
