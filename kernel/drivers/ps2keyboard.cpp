import arch_asm;
import gloxor.system;
import std.compat;
import gloxor.initgraph;
import glox;
#include "glox/assert.hpp"

static void kbd_handler()
{
    auto val = inb(0x60);
    gx::println("Kbval = %", val);
    gx::pic::send_eoi_master();
}

static void init_keyboard()
{
    gx::log_debug("Mapping Keyboard handler\r\n");
    gx::irq::attach_handler("ps2_keyboard", 0x21, kbd_handler);
    gx::pic::unmask_master_irq(1);
}

glox::initgraph::task ps2KeyboardTask {
    &gx::postCpuInit,
    "ps2_keyboard",
    glox::initgraph::require {
        gx::pic_init_stage(),
    },
    init_keyboard,
};
