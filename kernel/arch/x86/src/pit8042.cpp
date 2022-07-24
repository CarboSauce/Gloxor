#include "asm/pit8042.hpp"
#include "asm/asmstubs.hpp"
#include "asm/idt.hpp"
#include "asm/pic8042contr.hpp"
#include "gloxor/modules.hpp"
#include "system/logging.hpp"

static u64 tick = 0;
using namespace glox::pit;

namespace glox::pit
{
/**
 * @brief Set the divisor of PIT
 * Make sure to stop IRQ
 * @param divisor
 */
void setDivisor(u16 divisor)
{

	outb(0x40, divisor & 0xFF);
	ioWait();
	outb(0x40, (divisor & 0xFF00) >> 8);
}
} // namespace glox::pit

[[gnu::interrupt]] static void timerHandler([[maybe_unused]] interrupt_frame_t* frame)
{
	tick += 1;
	glox::pic::sendEoiMaster();
}

static void initTimer()
{
	auto idt = getIdt();
	gloxDebugLog("Mapping Timer handler\n");
	idt.base[0x20].registerHandler((uint64_t)timerHandler, 0x8, 0, IDT_INTERRUPTGATE);
	// Make sure we get 1ms interrupts
	outb(0x43, 0x36);
	glox::pit::setDivisor(glox::pit::timerFreq);
	// PIT is on line 0
	glox::pic::setMasterMask(0b11111110);
}

u64 getTicks()
{
	return tick;
}

void sleep(u64 curTicks, u64 ms)
{
	while (tick < curTicks + ms * baseFreq / (1000 * timerFreq))
		asm("hlt");
}

initDriverModule(initTimer);
