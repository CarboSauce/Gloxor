module;
#include "flanterm_backends/fb.h"
module gloxor.system.terminal;
import gloxor.graphics;
import gloxor.types;
import std.compat;
import glox;

using namespace gx;

extern "C++" const u8 fontBitmap[];

namespace gx::term {
static flanterm_context* flantermContext;
glox::span<u8> get_used_memory_range()
{
    return {
        (u8*)machineInfo.fbInfoEntry.begin,
        (u8*)machineInfo.fbInfoEntry.end,
    };
}

void init_term(const gx::BootInfo::FbInfo& fb)
{
    flantermContext = flanterm_fb_init(
        NULL,
        NULL,
        (uint32_t*)fb.begin,
        fb.width,
        fb.height,
        fb.pitch,
        fb.redMaskSize,
        fb.redMaskShift,
        fb.greenMaskSize,
        fb.greenMaskShift,
        fb.blueMaskSize,
        fb.blueMaskShift,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        (void*)&fontBitmap,
        8,
        16,
        1,
        0,
        0,
        0,
        0
    );
}
void write_str(const char* str, size_t size)
{
    flanterm_write(flantermContext, str, size);
}
void write_str(std::string_view str)
{
    flanterm_write(flantermContext, str.data(), str.length());
}
void clear_screen(color_t)
{
    flanterm_write(flantermContext, "\e[2J", 4);
}
} // namespace gx::term
