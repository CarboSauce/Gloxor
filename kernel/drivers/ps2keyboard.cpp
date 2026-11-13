import arch_asm;
import gloxor.system;
import std.compat;
import gloxor.initgraph;
import glox;
#include "glox/assert.hpp"
#include "system/logging.hpp"

[[gnu::interrupt]] static void kbd_handler(InterruptFrame* frame)
{
    auto val = inb(0x60);
    gx::println("Kbval = %\n", val);
    gx::pic::send_eoi_master();
}

static void init_keyboard()
{
    auto idt = get_idt();
    gx::log_debug("Mapping Keyboard handler\r\n");
    idt.base[0x21].register_handler(
        (uint64_t)kbd_handler, 0x8, 0, IDT_INTERRUPTGATE
    );

    // This should be generic, seperated drivers might have trouble to
    // unmask the controller
    gx::pic::set_master_mask(0b11111101);
}

glox::initgraph::task ps2KeyboardTask {
    &gx::postCpuInit,
    "ps2_keyboard",
    glox::initgraph::require {
        gx::pic_init_stage(),
    },
    init_keyboard,
};
