#include "asm/pit8042.hpp"
#include "asm/asmstubs.hpp"
#include "asm/idt.hpp"
#include "asm/pic8042contr.hpp"
#include "gloxor/modules.hpp"
#include "system/logging.hpp"

static u64 tick = 0;
using namespace gx::pit;

namespace gx::pit {
/**
 * @brief Set the divisor of PIT
 * Make sure to stop IRQ
 * @param divisor
 */
void set_divisor(u16 divisor)
{

	outb(0x40, divisor & 0xFF);
	io_wait();
	outb(0x40, (divisor & 0xFF00) >> 8);
}
} // namespace gx::pit

[[gnu::interrupt]] static void timer_handler([[maybe_unused]] InterruptFrame* frame)
{
	tick += 1;
	gx::pic::send_eoi_master();
}

static void init_timer()
{
	auto idt = get_idt();
	gloxDebugLog("Mapping Timer handler\n");
	idt.base[0x20].register_handler((uint64_t)timer_handler, 0x8, 0, IDT_INTERRUPTGATE);
	// Make sure we get 1ms interrupts
	outb(0x43, 0x36);
	gx::pit::set_divisor(gx::pit::timerFreq);
	// PIT is on line 0
	gx::pic::set_master_mask(0b11111110);
}

u64 get_ticks()
{
	return tick;
}

void sleep(u64 curTicks, u64 ms)
{
	while (tick < curTicks + ms * baseFreq / (1000 * timerFreq))
		asm("hlt");
}

initDriverModule(init_timer);
