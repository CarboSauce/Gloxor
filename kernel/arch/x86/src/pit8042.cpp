module arch_asm.pit8042;
import gloxor.system;
import arch_asm.asmstubs;
import arch_asm.idt;
import gloxor.initgraph;
import arch_asm.pic8042contr;
import std.compat;

static u64 volatile tick = 0;
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
    outb(0x40, divisor >> 8);
}
} // namespace gx::pit

static void timer_handler()
{
    tick += 1;
    gx::pic::send_eoi_master();
}

static void init_timer()
{
    gx::log_debug("Mapping Timer handler\r\n");
    gx::irq::attach_handler("pit_timer", 0x20, timer_handler);
    // Make sure we get 1ms interrupts
    outb(0x43, 0x36);
    gx::pit::set_divisor(gx::pit::timerFreq);
    // PIT is on line 0
    gx::pic::unmask_master_irq(0);
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

glox::initgraph::task pitTimerTask {
    &gx::postCpuInit,
    "pit_timer",
    glox::initgraph::require {
        gx::pic_init_stage(),
    },
    init_timer,
};
